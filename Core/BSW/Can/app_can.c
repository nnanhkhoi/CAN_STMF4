#include "app_can.h"
#include "main.h"
#include "can.h"
#include "CanTp.h"
#include <stdio.h>

typedef struct
{
  CAN_RxHeaderTypeDef header; /* CAN identifier, frame type, DLC, and RX metadata. */
  uint8_t data[8];            /* Received CAN payload (maximum classic CAN size). */
} can_rx_frame_t;

#define CAN_RX_QUEUE_CAPACITY 16U
#define CAN_RX_FRAMES_PER_ACTIVATION 4U
#define CAN_RX_FLAG_OVERFLOW  (1U << 0)
#define CAN_RX_FLAG_READ_ERROR (1U << 1)

/* RX ISR produces; 10 ms task consumes with a bounded critical section. */
static can_rx_frame_t can_rx_queue[CAN_RX_QUEUE_CAPACITY];
static uint32_t can_rx_head;
static uint32_t can_rx_tail;
static volatile uint32_t can_rx_count;
static volatile uint32_t can_rx_flags;
static volatile uint32_t can_rx_dropped;
static volatile uint32_t can_error_code;
static volatile uint8_t can_transport_lost; /* ISR detected data loss; task must abort reassembly. */



/* Remove one queued frame while preserving the caller's interrupt state. */
static uint8_t can_rx_pop(can_rx_frame_t *frame)
{
  uint32_t primask = __get_PRIMASK();
  uint8_t available = 0U;

  __disable_irq();
  if (can_rx_count != 0U)
  {
    *frame = can_rx_queue[can_rx_tail];
    can_rx_tail = (can_rx_tail + 1U) % CAN_RX_QUEUE_CAPACITY;
    --can_rx_count;
    available = 1U;
  }
  __set_PRIMASK(primask);
  return available;
}

/* Consume and process a bounded number of queued CAN frames every 10 ms. */
void App_CanMainFunction10ms(void)
{
  /* Discard the ambiguous queue after any gap: never deliver a partial request. */
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint8_t lost = can_transport_lost;
  can_transport_lost = 0U;
  if (lost != 0U)
  {
    can_rx_tail = can_rx_head;
    can_rx_count = 0U;
  }
  __set_PRIMASK(primask);
  if (lost != 0U) { CanTp_Cancel(); }

  /* Bounded workload even if CAN interrupts continuously refill the queue. */
  can_rx_frame_t frame;
  for (uint32_t i = 0; i < CAN_RX_FRAMES_PER_ACTIVATION; ++i)
  {
    if (can_transport_lost != 0U) { break; }
    if (!can_rx_pop(&frame))
    {
      break;
    }
    CanTp_RxIndication(frame.header.StdId, frame.data, (uint8_t)frame.header.DLC, HAL_GetTick());
    char buf[120];
    snprintf(buf, sizeof(buf),
      "[RX] ID=0x%03lX DLC=%lu Data=[%02X %02X %02X %02X %02X %02X %02X %02X]\r\n",
      (unsigned long)frame.header.StdId,
      (unsigned long)frame.header.DLC,
      frame.data[0], frame.data[1],
      frame.data[2], frame.data[3],
      frame.data[4], frame.data[5],
      frame.data[6], frame.data[7]);
    UART_Send(buf);
  }
  CanTp_MainFunction(HAL_GetTick());
}

/* Collect CAN diagnostics and enqueue a summary log at the 50 ms cadence. */
void App_CanDiagnostics50ms(void)
{
  static uint32_t last_error_tick;
  // Print CAN error if any (with detailed flags)
  if (HAL_GetTick() - last_error_tick >= 1000U)
  {
    last_error_tick = HAL_GetTick();
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t errors = can_error_code;
    uint32_t rx_flags = can_rx_flags;
    uint32_t dropped = can_rx_dropped;
    can_error_code = 0U;
    can_rx_flags = 0U;
    can_rx_dropped = 0U;
    __set_PRIMASK(primask);

    /* Formatting and log queuing always run with interrupts restored. */
    if (errors != 0U)
    {
      char buf[200];
      snprintf(buf, sizeof(buf), "[CAN ERR] 0x%08lX | Flags: %s%s%s%s%s%s%s%s%s%s\r\n",
        (unsigned long)errors,
        (errors & 0x01) ? "EWG " : "",      // Protocol Error Warning
        (errors & 0x02) ? "EPV " : "",      // Error Passive
        (errors & 0x04) ? "BOF " : "",      // Bus-off
        (errors & 0x08) ? "STF " : "",      // Stuff error
        (errors & 0x10) ? "FOR " : "",      // Form error
        (errors & 0x20) ? "ACK " : "",      // Acknowledgment error
        (errors & 0x40) ? "BR " : "",       // Bit recessive error
        (errors & 0x80) ? "BD " : "",       // Bit dominant error
        (errors & 0x100) ? "CRC " : "",     // CRC error
        (errors & HAL_CAN_ERROR_RX_FOV0) ? "RX_FOV0 " : ""
      );
      UART_Send(buf);
    }
    if (rx_flags != 0U)
    {
      char buf[120];
      snprintf(buf, sizeof(buf), "[CAN RX] Flags: %s%s dropped=%lu\r\n",
        (rx_flags & CAN_RX_FLAG_OVERFLOW) ? "QUEUE_OVERFLOW " : "",
        (rx_flags & CAN_RX_FLAG_READ_ERROR) ? "READ_ERROR " : "",
        (unsigned long)dropped);
      UART_Send(buf);
    }
  }
}

/* Interrupt Callbacks */

/**
 * @brief  Rx FIFO 0 message pending callback.
 * @param  hcan pointer to a CAN_HandleTypeDef structure that contains
 *         the configuration information for the specified CAN.
 * @retval None
 */
/* Read one hardware FIFO frame and queue it for task-context processing. */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  if (hcan != &hcan1) { return; }
  can_rx_frame_t frame = {0};

  /* Exactly one read per callback. Never wait for, or drain, the FIFO. */
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &frame.header, frame.data) != HAL_OK)
  {
    can_rx_flags |= CAN_RX_FLAG_READ_ERROR;
    can_error_code |= HAL_CAN_GetError(hcan);
    can_transport_lost = 1U;
    return;
  }

  /* Reject unrelated/extended/remote traffic before it consumes software slots. */
  if (frame.header.IDE != CAN_ID_STD || frame.header.RTR != CAN_RTR_DATA ||
      frame.header.StdId != CANTP_RX_ID || frame.header.DLC > 8U)
  {
    return;
  }

  /* Read first so a full software queue still releases the hardware FIFO. */
  if (can_rx_count == CAN_RX_QUEUE_CAPACITY)
  {
    can_rx_flags |= CAN_RX_FLAG_OVERFLOW;
    can_transport_lost = 1U;
    if (can_rx_dropped != UINT32_MAX)
    {
      ++can_rx_dropped;
    }
    return;
  }

  can_rx_queue[can_rx_head] = frame;
  can_rx_head = (can_rx_head + 1U) % CAN_RX_QUEUE_CAPACITY;
  __DMB();
  ++can_rx_count;
}

/* Capture CAN error flags for later reporting by the diagnostics task. */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
  if (hcan != &hcan1) { return; }
  can_error_code |= HAL_CAN_GetError(hcan);
  if ((HAL_CAN_GetError(hcan) & (HAL_CAN_ERROR_RX_FOV0 | HAL_CAN_ERROR_BOF)) != 0U)
  {
    can_transport_lost = 1U;
  }
}
