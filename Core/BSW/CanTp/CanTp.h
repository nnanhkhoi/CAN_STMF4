#ifndef CANTP_H
#define CANTP_H

#include <stdbool.h>
#include <stdint.h>
#include "CanTp_Cfg.h"

typedef enum
{
  CANTP_OK,       /* The complete payload was copied into the TX buffer. */
  CANTP_BUSY,     /* A transfer is active; retry later from the owning task. */
  CANTP_INVALID  /* Invalid pointer, length, or module not initialized. */
} CanTp_Result_t;

typedef struct
{
  uint32_t rx_complete; /* Successfully reassembled payloads delivered upward. */
  uint32_t tx_complete; /* Payloads whose final CAN frame was confirmed. */
  uint32_t rx_errors;   /* Malformed, unexpected, oversized, or lost RX transfers. */
  uint32_t tx_errors;   /* Transfers aborted by peer rejection or CAN failure. */
  uint32_t timeouts;    /* N_As, N_Ar, N_Bs, or N_Cr expirations. */
  uint32_t tx_rejected; /* Transmit requests rejected as busy or invalid. */
} CanTp_Stats_t;

/* Initialize once before the scheduler; all subsequent APIs use the 10 ms task. */
void CanTp_Init(void);
/* Copy an entire UDS payload; OK means accepted, not yet transmitted on the bus. */
CanTp_Result_t CanTp_Transmit(const uint8_t *data, uint16_t length, uint32_t now_ms);
/* Feed one standard data frame in task context; unrelated identifiers are ignored. */
void CanTp_RxIndication(uint32_t can_id, const uint8_t *data, uint8_t dlc, uint32_t now_ms);
/* Poll CAN confirmation, enforce timeouts, and submit at most one CAN frame. */
void CanTp_MainFunction(uint32_t now_ms);
/* Abort active transfers after RX queue loss, hardware FIFO overrun, or bus-off. */
void CanTp_Cancel(void);
/* Return counters; call only from the same task that owns the transport. */
const CanTp_Stats_t *CanTp_GetStats(void);
/* Upper-layer callback: data is valid during this call only; never retain it. */
void CanTp_RxComplete(uint8_t *data, uint16_t length);
/* Upper-layer callback after final confirmation or failure; runs in task context. */
void CanTp_TxComplete(bool success);

#endif
