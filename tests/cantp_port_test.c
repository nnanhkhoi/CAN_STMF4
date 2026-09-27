#include "can.h"
#include "CanTp_Port.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static CAN_TypeDef registers;
CAN_HandleTypeDef hcan1 = {&registers};
static bool pending;
static uint32_t free_count = 3U;
static uint32_t selected = CAN_TX_MAILBOX0;
static unsigned aborts;
static CAN_TxHeaderTypeDef captured_header;
static uint8_t captured_data[8];

/* Return the mocked controller's outstanding request state. */
uint32_t HAL_CAN_IsTxMessagePending(CAN_HandleTypeDef *handle, uint32_t mailbox)
{
  assert(handle == &hcan1 && mailbox == selected);
  return pending;
}

/* Let a test independently simulate exhaustion of all hardware mailboxes. */
uint32_t HAL_CAN_GetTxMailboxesFreeLevel(CAN_HandleTypeDef *handle)
{
  assert(handle == &hcan1);
  return free_count;
}

/* Capture a submission without completing it. */
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *handle, CAN_TxHeaderTypeDef *header,
                                     uint8_t *data, uint32_t *mailbox)
{
  assert(handle == &hcan1);
  captured_header = *header;
  memcpy(captured_data, data, 8U);
  *mailbox = selected;
  pending = true;
  registers.TSR = 0U;
  return HAL_OK;
}

/* Aborting is asynchronous: keep TX pending until the test clears it. */
HAL_StatusTypeDef HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *handle, uint32_t mailbox)
{
  assert(handle == &hcan1 && mailbox == selected);
  ++aborts;
  return HAL_OK;
}

/* Check real success bits, mailbox selection, cancellation races and bus-off. */
int main(void)
{
  const uint8_t data[8] = {2U, 0x50U, 3U};
  for (unsigned i = 0; i < 3U; ++i)
  {
    selected = 1U << i;
    assert(CanTp_PortSend(0x7E8U, data) == CANTP_PORT_OK);
    assert(captured_header.StdId == 0x7E8U && captured_header.DLC == 8U);
    assert(memcmp(captured_data, data, 8U) == 0);
    assert(CanTp_PortPoll() == CANTP_PORT_BUSY);
    assert(CanTp_PortSend(0x7E8U, data) == CANTP_PORT_BUSY);
    pending = false;
    registers.TSR = (CAN_TSR_RQCP0 | CAN_TSR_TXOK0) << (8U * i);
    assert(CanTp_PortPoll() == CANTP_PORT_OK);
    assert(registers.TSR == (CAN_TSR_RQCP0 << (8U * i)));
    assert(CanTp_PortSend(0x7E8U, data) == CANTP_PORT_OK);
    pending = false; registers.TSR = CAN_TSR_RQCP0 << (8U * i);
    assert(CanTp_PortPoll() == CANTP_PORT_ERROR); /* Empty is not success. */
  }
  assert(CanTp_PortSend(0x7E8U, data) == CANTP_PORT_OK);
  CanTp_PortCancel();
  assert(aborts == 1U);
  assert(CanTp_PortSend(0x7E8U, data) == CANTP_PORT_BUSY);
  pending = false;
  assert(CanTp_PortSend(0x7E8U, data) == CANTP_PORT_OK);
  CanTp_PortCancel();
  pending = false;
  registers.TSR = (CAN_TSR_RQCP0 | CAN_TSR_TXOK0) << 16;
  assert(CanTp_PortPoll() == CANTP_PORT_ERROR); /* A late TXOK cannot revive an aborted transfer. */
  free_count = 0U;
  assert(CanTp_PortSend(0x7E8U, data) == CANTP_PORT_BUSY);
  free_count = 3U; registers.ESR = CAN_ESR_BOFF;
  assert(CanTp_PortSend(0x7E8U, data) == CANTP_PORT_ERROR);
  puts("CanTp bxCAN port tests passed");
  return 0;
}
