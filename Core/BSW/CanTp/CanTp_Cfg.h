#ifndef CANTP_CFG_H
#define CANTP_CFG_H

/* One physical ISO-TP channel, classic CAN, normal 11-bit addressing. */
#define CANTP_RX_ID              0x7E0U
#define CANTP_TX_ID              0x7E8U
#define CANTP_MAX_PAYLOAD        1024U /* Static RX and TX buffer size, 8..4095 bytes. */
#define CANTP_RX_BLOCK_SIZE      4U    /* CFs allowed before another CTS. */
#define CANTP_RX_STMIN           10U   /* Requested CF spacing in milliseconds. */
#define CANTP_MAX_WAIT_FRAMES     3U    /* Abort TX on the fourth consecutive FC WAIT. */
#define CANTP_N_AS_MS            1000U /* Deadline for submitting/confirming a data frame. */
#define CANTP_N_AR_MS            1000U /* Deadline for submitting/confirming flow control. */
#define CANTP_N_BS_MS            1000U /* Maximum wait for peer flow control. */
#define CANTP_N_CR_MS            1000U /* Maximum wait for the next consecutive frame. */
#define CANTP_PADDING_BYTE      0x00U /* Outgoing frames always have DLC 8. */

#if CANTP_MAX_PAYLOAD < 8U || CANTP_MAX_PAYLOAD > 4095U
#error "Classic 12-bit ISO-TP length requires a buffer size between 8 and 4095"
#endif
#if CANTP_RX_ID > 0x7FFU || CANTP_TX_ID > 0x7FFU || CANTP_RX_ID == CANTP_TX_ID
#error "CanTp requires two distinct standard CAN identifiers"
#endif
#if CANTP_RX_BLOCK_SIZE > 255U || CANTP_RX_STMIN > 127U
#error "RX flow control configuration is outside the supported range"
#endif

#endif
