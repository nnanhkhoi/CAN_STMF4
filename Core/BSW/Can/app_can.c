#include "app_can.h"
#include "main.h"
#include "can.h"
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

static void process_can_frame(can_rx_frame_t *frame);

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
  /* Bounded workload even if CAN interrupts continuously refill the queue. */
  can_rx_frame_t frame;
  for (uint32_t i = 0; i < CAN_RX_FRAMES_PER_ACTIVATION; ++i)
  {
    if (!can_rx_pop(&frame))
    {
      break;
    }
    process_can_frame(&frame);
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
  can_rx_frame_t frame = {0};

  /* Exactly one read per callback. Never wait for, or drain, the FIFO. */
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &frame.header, frame.data) != HAL_OK)
  {
    can_rx_flags |= CAN_RX_FLAG_READ_ERROR;
    can_error_code |= HAL_CAN_GetError(hcan);
    return;
  }

  /* Read first so a full software queue still releases the hardware FIFO. */
  if (can_rx_count == CAN_RX_QUEUE_CAPACITY)
  {
    can_rx_flags |= CAN_RX_FLAG_OVERFLOW;
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

/* 10 ms task only: parsing, service execution and response transmission. */
/* Dispatch a validated CAN frame to its matching UDS service handler. */
static void process_can_frame(can_rx_frame_t *frame)
{
  uint8_t *rcvd_msg = frame->data;
  uint8_t response[3];

  /* The existing dispatcher expects a SID and at least one parameter byte. */
  if (frame->header.RTR != CAN_RTR_DATA ||
      frame->header.DLC < 2U || frame->header.DLC > sizeof(frame->data))
  {
    return;
  }

  switch (rcvd_msg[0])
  {
  case UDS_DIAGNOSTIC_SESSION_CONTROL:
    // Call the Diagnostic Session Control service handler
    uds_diagnostic_session_control(rcvd_msg[1]);
    break;

  case UDS_ECU_RESET:
    // Call the ECU Reset service handler with resetType
    uds_ecu_reset(rcvd_msg[1]);
    break;
  case UDS_SECURITY_ACCESS:
    // Call the Security Access service handler
    uds_security_access(rcvd_msg[1], &rcvd_msg[2], frame->header.DLC - 2);
    break;
  case UDS_COMMUNICATION_CONTROL:
    uds_communication_control(rcvd_msg[1]);
    break;
  case UDS_TESTER_PRESENT:
    // Call the TesterPresent service handler
    uds_tester_present(rcvd_msg[1]);
    break;
  case UDS_ACCESS_TIMING_PARAMETER:
    // Call the Access Timing Parameter service handler
    uds_access_timing_parameter(rcvd_msg[1], &rcvd_msg[2], frame->header.DLC - 2);
    break;
  case UDS_SECURED_DATA_TRANSMISSION:
    // Call the Secured Data Transmission service handler
    uds_secured_data_transmission(&rcvd_msg[1], frame->header.DLC - 1);
    break;
  case UDS_CONTROL_DTC_SETTING:
    // Call the ControlDTCSetting service handler
    uds_control_dtc_setting(rcvd_msg[1]);
    break;
  case UDS_RESPONSE_ON_EVENT:
    // Call the ResponseOnEvent service handler
    uds_response_on_event(rcvd_msg[1], &rcvd_msg[2], frame->header.DLC - 2);
    break;
  case UDS_LINK_CONTROL:
    // Call the LinkControl service handler
    uds_link_control(rcvd_msg[1], &rcvd_msg[2], frame->header.DLC - 2);
    break;
  case UDS_READ_DATA_BY_IDENTIFIER:
    // Call the ReadDataByIdentifier service handler
    uds_read_data_by_identifier(&rcvd_msg[1], frame->header.DLC - 1);
    break;
  case UDS_READ_DATA_BY_PERIODIC_IDENTIFIER:
    // Call the ReadDataByPeriodicIdentifier service handler
    uds_read_data_by_periodic_identifier(&rcvd_msg[1], frame->header.DLC - 1);
    break;
  case UDS_DYNAMICAL_DEFINE_DATA_IDENTIFIER:
    // Call the DynamicallyDefineDataIdentifier service handler
    uds_dynamically_define_data_identifier(rcvd_msg[1], &rcvd_msg[2], frame->header.DLC - 2);
    break;
  case UDS_WRITE_DATA_BY_IDENTIFIER:
    // Call the WriteDataByIdentifier service handler
    uds_write_data_by_identifier(&rcvd_msg[1], frame->header.DLC - 1);
    break;
  case UDS_CLEAR_DIAGNOSTIC_INFORMATION:
    // Call the ClearDiagnosticInformation service handler
    uds_clear_diagnostic_information(&rcvd_msg[1], frame->header.DLC - 1);
    break;
  case UDS_READ_DTC_INFORMATION:
    // Appeler la fonction pour gérer le service ReadDTCInformation
    uds_read_dtc_information(rcvd_msg[1], &rcvd_msg[2], frame->header.DLC - 2);
    break;
  case UDS_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER:
    uds_input_output_control_by_identifier((IOControlRequest_t *)&rcvd_msg[1], NULL);
    break;
  case UDS_ROUTINE_CONTROL:
    uds_routine_control((RoutineControlRequest_t *)&rcvd_msg[1], NULL);
    break;
  case UDS_REQUEST_DOWNLOAD:
    uds_request_download((RequestDownload_t *)&rcvd_msg[1]);
    break;
  case UDS_REQUEST_UPLOAD:
    uds_request_upload((RequestUpload_t *)&rcvd_msg[1]);
    break;
  case UDS_TRANSFER_DATA:
    uds_transfer_data((RequestTransferData_t *)&rcvd_msg[1]);
    break;
  case UDS_REQUEST_TRANSFER_EXIT:
    uds_request_transfer_exit((RequestTransferExit_t *)&rcvd_msg[1], NULL);
    break;
  case UDS_REQUEST_FILE_TRANSFER:
    uds_request_file_transfer((RequestFileTransfer_t *)&rcvd_msg[1]);
    break;

  default:
    // Remplir le message de réponse négative pour un service non supporté
    response[0] = UDS_NEGATIVE_RESPONSE;     // Réponse négative générique
    response[1] = rcvd_msg[0];               // Service non supporté
    response[2] = NRC_SERVICE_NOT_SUPPORTED; // Code NRC (ServiceNotSupported)

    // Envoyer le message de réponse négative via CAN
    send_can_message(response, 3);
    break;
  }
}
/**
 * @brief  Transmission Mailbox 0 complete callback.
 * @param  hcan pointer to a CAN_HandleTypeDef structure that contains
 *         the configuration information for the specified CAN.
 * @retval None
 */
/* HAL callback invoked when CAN transmit mailbox 0 completes. */
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
  // TX complete — flag could be added here too if needed
}

/* Capture CAN error flags for later reporting by the diagnostics task. */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
  can_error_code |= HAL_CAN_GetError(hcan);
}
