#ifndef CANTP_PORT_H
#define CANTP_PORT_H

#include <stdint.h>

typedef enum
{
  CANTP_PORT_OK,    /* Frame accepted (Send) or actually completed (Poll). */
  CANTP_PORT_BUSY,  /* Mailbox unavailable (Send) or still transmitting (Poll). */
  CANTP_PORT_ERROR  /* Controller failed or the frame was aborted. */
} CanTp_PortResult_t;

/* Submit one eight-byte frame without waiting; the port copies data immediately. */
CanTp_PortResult_t CanTp_PortSend(uint32_t can_id, const uint8_t data[8]);
/* Poll the owned mailbox; OK must mean successful transmission, not just empty. */
CanTp_PortResult_t CanTp_PortPoll(void);
/* Request hardware cancellation without waiting for it to complete. */
void CanTp_PortCancel(void);

#endif
