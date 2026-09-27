#include "CanTp_Port.h"
#include "can.h"
#include <stdbool.h>

static uint32_t mailbox;       /* HAL mailbox bit mask owned by this transport. */
static bool owned;             /* A submission or cancellation is still outstanding. */
static bool cancelling;        /* Abort was requested; completion must not be reported as OK. */

/* Read the bxCAN result before clearing RQCP; TX-empty notifications stay disabled. */
CanTp_PortResult_t CanTp_PortPoll(void)
{
  if (!owned) { return CANTP_PORT_ERROR; }
  if (HAL_CAN_IsTxMessagePending(&hcan1, mailbox) != 0U) { return CANTP_PORT_BUSY; }
  uint32_t shift = mailbox == CAN_TX_MAILBOX0 ? 0U :
                   (mailbox == CAN_TX_MAILBOX1 ? 8U : 16U);
  uint32_t status = hcan1.Instance->TSR;
  bool success = !cancelling && (status & (CAN_TSR_TXOK0 << shift)) != 0U;
  /* Write-one-to-clear only the completion flag of the mailbox we own. */
  hcan1.Instance->TSR = CAN_TSR_RQCP0 << shift;
  owned = false;
  cancelling = false;
  return success ? CANTP_PORT_OK : CANTP_PORT_ERROR;
}

/* Request transmission once; never wait for a mailbox or for an ACK on the bus. */
CanTp_PortResult_t CanTp_PortSend(uint32_t can_id, const uint8_t data[8])
{
  /* A previous abort may take time in hardware; do not overwrite that mailbox. */
  if (owned)
  {
    if (!cancelling || CanTp_PortPoll() == CANTP_PORT_BUSY) { return CANTP_PORT_BUSY; }
  }
  if ((hcan1.Instance->ESR & CAN_ESR_BOFF) != 0U) { return CANTP_PORT_ERROR; }
  if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U) { return CANTP_PORT_BUSY; }
  CAN_TxHeaderTypeDef header = {0};
  header.StdId = can_id;
  header.IDE = CAN_ID_STD;
  header.RTR = CAN_RTR_DATA;
  header.DLC = 8U;
  header.TransmitGlobalTime = DISABLE;
  /* HAL copies these bytes into mailbox registers before returning. */
  if (HAL_CAN_AddTxMessage(&hcan1, &header, (uint8_t *)data, &mailbox) != HAL_OK)
  {
    return CANTP_PORT_ERROR;
  }
  owned = true;
  cancelling = false;
  return CANTP_PORT_OK;
}

/* Abort only our mailbox, retaining ownership until hardware finishes the abort. */
void CanTp_PortCancel(void)
{
  if (owned)
  {
    (void)HAL_CAN_AbortTxRequest(&hcan1, mailbox);
    cancelling = true;
  }
}
