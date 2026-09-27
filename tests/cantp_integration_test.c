#include "CanTp.h"
#include "CanTp_Port.h"
#include "uds_services.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

CAN_HandleTypeDef hcan1;
static uint32_t tick;
static unsigned resets, sent_count;
static bool active, completed;
static uint8_t sent[16][8];

/* Supply deterministic time to the production UDS response adapter. */
uint32_t HAL_GetTick(void) { return tick; }
/* Observe a reset instead of resetting the host running the tests. */
void NVIC_SystemReset(void) { ++resets; }
/* Do not enter a host power state during diagnostic tests. */
void HAL_PWR_EnterSTOPMode(uint32_t regulator, uint8_t entry) { (void)regulator; (void)entry; }
/* Suppress UART output from legacy demonstration handlers. */
void UART_Send(const char *message) { (void)message; }
/* A firmware fatal error must fail the host test instead of spinning forever. */
void Error_Handler(void) { assert(!"Unexpected firmware Error_Handler"); }

/* Capture complete CAN frames submitted by the production transport. */
CanTp_PortResult_t CanTp_PortSend(uint32_t id, const uint8_t data[8])
{
  assert(id == CANTP_TX_ID && sent_count < 16U);
  if (active) { return CANTP_PORT_BUSY; }
  memcpy(sent[sent_count++], data, 8U);
  active = true; completed = false;
  return CANTP_PORT_OK;
}

/* Confirm transmission only when the scenario explicitly acknowledges it. */
CanTp_PortResult_t CanTp_PortPoll(void)
{
  assert(active);
  if (!completed) { return CANTP_PORT_BUSY; }
  active = false;
  return CANTP_PORT_OK;
}

/* Drop the simulated mailbox when the transport times out. */
void CanTp_PortCancel(void) { active = false; }

/* Reset the protocol and platform between independent test scenarios. */
static void Reset(void)
{
  CanTp_Cancel();
  CanTp_Init();
  tick = 0U; resets = sent_count = 0U;
  active = completed = false;
}

/* Inject a classic CAN frame then service the cyclic sender at this instant. */
static void Receive(const uint8_t frame[8], uint32_t now)
{
  tick = now;
  CanTp_RxIndication(CANTP_RX_ID, frame, 8U, tick);
  CanTp_MainFunction(tick);
}

/* Acknowledge an outstanding frame at a chosen time. */
static void Complete(uint32_t now)
{
  tick = now; completed = true;
  CanTp_MainFunction(tick);
}

/* Verify an exact byte trace from README against the actual UDS service code. */
static void Expect(unsigned index, const uint8_t frame[8])
{
  assert(index < sent_count && memcmp(sent[index], frame, 8U) == 0);
}

/* Exercise complete UDS/CanTp paths, length checks and post-response reset timing. */
int main(void)
{
  Reset();
  Receive((uint8_t[]){2, 0x10, 3, 0, 0, 0, 0, 0}, 0U);
  Expect(0U, (uint8_t[]){6, 0x50, 3, 0, 0x32, 1, 0xF4, 0});
  Complete(1U);

  Reset();
  Receive((uint8_t[]){3, 0x22, 0xF1, 0x87, 0, 0, 0, 0}, 0U);
  Expect(0U, (uint8_t[]){0x10, 0x0C, 0x62, 0xF1, 0x87, 0xDE, 0xAD, 0xAD});
  Complete(10U);
  Receive((uint8_t[]){0x30, 0, 10, 0, 0, 0, 0, 0}, 10U);
  tick = 20U; CanTp_MainFunction(tick);
  Expect(1U, (uint8_t[]){0x21, 0xAD, 0xAD, 0xAD, 0xAD, 0xAD, 0xAD, 0});
  Complete(21U);

  Reset();
  Receive((uint8_t[]){0x10, 9, 0x22, 0x10, 1, 0x10, 2, 0x10}, 0U);
  Expect(0U, (uint8_t[]){0x30, 4, 10, 0, 0, 0, 0, 0});
  Complete(1U);
  Receive((uint8_t[]){0x21, 3, 0x10, 1, 0, 0, 0, 0}, 11U);
  Expect(1U, (uint8_t[]){0x10, 0x11, 0x62, 0x10, 1, 0x12, 0x34, 0x10});
  Complete(12U);
  Receive((uint8_t[]){0x30, 0, 0, 0, 0, 0, 0, 0}, 12U);
  Expect(2U, (uint8_t[]){0x21, 2, 0x56, 0x78, 0x10, 3, 0x9A, 0xBC});
  Complete(13U);
  Expect(3U, (uint8_t[]){0x22, 0x10, 1, 0x12, 0x34, 0, 0, 0});
  Complete(14U);
  assert(CanTp_GetStats()->rx_complete == 1U && CanTp_GetStats()->tx_complete == 1U);

  Reset();
  Receive((uint8_t[]){2, 0x31, 1, 0, 0, 0, 0, 0}, 0U);
  Expect(0U, (uint8_t[]){3, 0x7F, 0x31, 0x13, 0, 0, 0, 0});
  Complete(1U);
  Reset();
  uint8_t large_request[300] = {0x22};
  CanTp_RxComplete(large_request, sizeof(large_request));
  CanTp_MainFunction(0U);
  Expect(0U, (uint8_t[]){3, 0x7F, 0x22, 0x13, 0, 0, 0, 0});
  Complete(1U);
  Reset();
  uint8_t repeated_dids[] = {0x22, 0xF1, 0x87, 0xF1, 0x87, 0xF1, 0x87,
                            0xF1, 0x87, 0xF1, 0x87, 0xF1, 0x87};
  CanTp_RxComplete(repeated_dids, sizeof(repeated_dids));
  CanTp_MainFunction(0U);
  Expect(0U, (uint8_t[]){3, 0x7F, 0x22, 0x14, 0, 0, 0, 0});
  Complete(1U);
  Reset();
  Receive((uint8_t[]){3, 0x28, 0, 1, 0, 0, 0, 0}, 0U);
  Expect(0U, (uint8_t[]){3, 0x7F, 0x28, 0x22, 0, 0, 0, 0});
  Complete(1U);
  Reset();
  Receive((uint8_t[]){2, 0x11, 1, 0, 0, 0, 0, 0}, 0U);
  assert(resets == 0U);
  Complete(1U);
  assert(resets == 1U);
  Reset();
  Receive((uint8_t[]){2, 0x11, 1, 0, 0, 0, 0, 0}, 0U);
  tick = CANTP_N_AS_MS; CanTp_MainFunction(tick);
  assert(resets == 0U && CanTp_GetStats()->tx_errors == 1U);
  puts("CanTp / UDS integration tests passed");
  return 0;
}
