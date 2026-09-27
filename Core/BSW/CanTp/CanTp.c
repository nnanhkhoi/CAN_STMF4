#include "CanTp.h"
#include "CanTp_Port.h"
#include <string.h>

typedef enum { TX_IDLE, TX_FIRST, TX_WAIT_FC, TX_CF } TxState_t;
typedef enum { RX_IDLE, RX_FC, RX_CF, RX_OVERFLOW } RxState_t;
typedef enum { FRAME_NONE, FRAME_SF, FRAME_FF, FRAME_CF, FRAME_FC, FRAME_OVERFLOW } Frame_t;

typedef struct
{
  TxState_t state;                    /* Current sender state. */
  uint8_t data[CANTP_MAX_PAYLOAD];     /* Owned copy of the complete outgoing payload. */
  uint16_t length;                    /* Payload length in bytes. */
  uint16_t offset;                    /* Bytes already submitted to the CAN controller. */
  uint8_t sequence;                   /* Next CF sequence number, modulo 16. */
  uint8_t block_size;                 /* Peer block size; zero means unlimited. */
  uint8_t block_count;                /* CFs confirmed in the current block. */
  uint8_t wait_count;                 /* Consecutive FC WAIT frames received. */
  uint32_t stmin_ms;                  /* Peer minimum spacing rounded upward to ms. */
  uint32_t since_ms;                  /* Start of the current timeout interval. */
  uint32_t confirmed_ms;              /* Time the previous frame was observed complete. */
} TxContext_t;

typedef struct
{
  RxState_t state;                    /* Current receiver state. */
  uint8_t data[CANTP_MAX_PAYLOAD];     /* Reassembly storage owned by CanTp. */
  uint16_t length;                    /* Complete payload length from the FF. */
  uint16_t offset;                    /* Bytes copied into the reassembly buffer. */
  uint8_t sequence;                   /* Expected CF sequence number, modulo 16. */
  uint8_t block_count;                /* CFs received since the last CTS. */
  uint32_t since_ms;                  /* Start of N_Ar or N_Cr. */
} RxContext_t;

static TxContext_t tx;
static RxContext_t rx;
static CanTp_Stats_t stats;
static Frame_t pending;
static uint32_t pending_since_ms;
static bool initialized;

/* Increment diagnostics without wrapping after prolonged operation. */
static void Count(uint32_t *counter)
{
  if (*counter != UINT32_MAX) { ++*counter; }
}

/* Unsigned subtraction keeps timeout checks valid across HAL tick rollover. */
static bool Expired(uint32_t now, uint32_t since, uint32_t interval)
{
  return (uint32_t)(now - since) >= interval;
}

/* Abort the local RX transfer, including an outstanding flow-control mailbox. */
static void AbortRx(void)
{
  if (pending == FRAME_FC || pending == FRAME_OVERFLOW)
  {
    CanTp_PortCancel();
    pending = FRAME_NONE;
  }
  rx.state = RX_IDLE;
  Count(&stats.rx_errors);
}

/* Abort the sender and release its buffer for the next Transmit request. */
static void AbortTx(void)
{
  if (pending == FRAME_SF || pending == FRAME_FF || pending == FRAME_CF)
  {
    CanTp_PortCancel();
    pending = FRAME_NONE;
  }
  tx.state = TX_IDLE;
  Count(&stats.tx_errors);
  CanTp_TxComplete(false);
}

/* Decode peer STmin; sub-ms requests round up and reserved values use 127 ms. */
static uint32_t DecodeStmin(uint8_t value)
{
  if (value <= 0x7FU) { return value; }
  if (value >= 0xF1U && value <= 0xF9U) { return 1U; }
  return 127U;
}

/* Advance a transfer only after the hardware confirms its last CAN frame. */
static void PollConfirmation(uint32_t now)
{
  if (pending == FRAME_NONE) { return; }
  CanTp_PortResult_t result = CanTp_PortPoll();
  bool flow = pending == FRAME_FC || pending == FRAME_OVERFLOW;
  if (result == CANTP_PORT_BUSY)
  {
    if (!Expired(now, pending_since_ms, flow ? CANTP_N_AR_MS : CANTP_N_AS_MS)) { return; }
    Count(&stats.timeouts);
    CanTp_PortCancel();
    result = CANTP_PORT_ERROR;
  }
  if (result == CANTP_PORT_ERROR)
  {
    if (flow) { AbortRx(); } else { AbortTx(); }
    return;
  }

  Frame_t completed = pending;
  pending = FRAME_NONE;
  tx.confirmed_ms = now;
  if (completed == FRAME_FC)
  {
    rx.state = RX_CF;
    rx.since_ms = now;
  }
  else if (completed == FRAME_OVERFLOW)
  {
    rx.state = RX_IDLE;
  }
  else if (completed == FRAME_SF || (completed == FRAME_CF && tx.offset == tx.length))
  {
    tx.state = TX_IDLE;
    Count(&stats.tx_complete);
    CanTp_TxComplete(true);
  }
  else if (completed == FRAME_FF ||
           (completed == FRAME_CF && ++tx.block_count == tx.block_size && tx.block_size != 0U))
  {
    tx.state = TX_WAIT_FC;
    tx.since_ms = now;
    tx.wait_count = 0U;
  }
  else
  {
    tx.state = TX_CF;
    tx.since_ms = now;
  }
}

/* Clear the state machine and statistics before its first scheduled activation. */
void CanTp_Init(void)
{
  CanTp_PortCancel();
  memset(&tx, 0, sizeof(tx));
  memset(&rx, 0, sizeof(rx));
  memset(&stats, 0, sizeof(stats));
  pending = FRAME_NONE;
  initialized = true;
}

/* Accept one payload by value; caller buffers may be stack allocated or reused. */
CanTp_Result_t CanTp_Transmit(const uint8_t *data, uint16_t length, uint32_t now_ms)
{
  if (!initialized || data == NULL || length == 0U || length > CANTP_MAX_PAYLOAD)
  {
    Count(&stats.tx_rejected);
    return CANTP_INVALID;
  }
  if (tx.state != TX_IDLE || rx.state != RX_IDLE || pending != FRAME_NONE)
  {
    Count(&stats.tx_rejected);
    return CANTP_BUSY;
  }
  memcpy(tx.data, data, length);
  tx.length = length;
  tx.offset = 0U;
  tx.sequence = 1U;
  tx.block_count = 0U;
  tx.state = TX_FIRST;
  tx.since_ms = now_ms;
  return CANTP_OK;
}

/* Decode flow-control frames only while awaiting the next sender block. */
static void ReceiveFlowControl(const uint8_t *data, uint8_t dlc, uint32_t now)
{
  if (tx.state != TX_WAIT_FC) { return; }
  if (Expired(now, tx.since_ms, CANTP_N_BS_MS))
  {
    Count(&stats.timeouts);
    AbortTx();
    return;
  }
  if (dlc < 3U) { AbortTx(); return; }
  switch (data[0] & 0x0FU)
  {
    case 0U: /* CTS: new BS and STmin apply to the next block. */
      tx.block_size = data[1];
      tx.stmin_ms = DecodeStmin(data[2]);
      tx.block_count = 0U;
      tx.wait_count = 0U;
      tx.state = TX_CF;
      tx.since_ms = now;
      break;
    case 1U: /* WAIT restarts N_Bs, but never permits an unbounded wait. */
      if (++tx.wait_count > CANTP_MAX_WAIT_FRAMES) { AbortTx(); }
      else { tx.since_ms = now; }
      break;
    default: /* Overflow (2) or reserved flow status ends this transmission. */
      AbortTx();
      break;
  }
}

/* Process SF/FF/CF/FC from the configured physical peer in task context. */
void CanTp_RxIndication(uint32_t can_id, const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
  if (!initialized || can_id != CANTP_RX_ID) { return; }
  PollConfirmation(now_ms);
  if (data == NULL || dlc == 0U || dlc > 8U) { AbortRx(); return; }
  uint8_t type = data[0] >> 4;
  if (type == 3U) { ReceiveFlowControl(data, dlc, now_ms); return; }
  /* This ECU channel is half duplex: a new request cannot overwrite a response. */
  if (tx.state != TX_IDLE) { Count(&stats.rx_errors); return; }
  if (type == 0U || type == 1U)
  {
    if (rx.state != RX_IDLE) { AbortRx(); }
    if (type == 0U)
    {
      uint8_t length = data[0] & 0x0FU;
      if (length == 0U || length > 7U || dlc < length + 1U) { AbortRx(); return; }
      memcpy(rx.data, data + 1, length);
      Count(&stats.rx_complete);
      CanTp_RxComplete(rx.data, length);
    }
    else
    {
      if (dlc != 8U) { AbortRx(); return; }
      rx.length = (uint16_t)(((uint16_t)(data[0] & 0x0FU) << 8) | data[1]);
      /* Zero indicates the unsupported 32-bit FF length extension. */
      if (rx.length < 8U) { AbortRx(); return; }
      rx.since_ms = now_ms;
      if (rx.length > CANTP_MAX_PAYLOAD)
      {
        Count(&stats.rx_errors);
        rx.state = RX_OVERFLOW;
        return;
      }
      memcpy(rx.data, data + 2, 6U);
      rx.offset = 6U;
      rx.sequence = 1U;
      rx.block_count = 0U;
      rx.state = RX_FC;
    }
    return;
  }
  if (type != 2U || rx.state != RX_CF) { AbortRx(); return; }
  if (Expired(now_ms, rx.since_ms, CANTP_N_CR_MS))
  {
    Count(&stats.timeouts);
    AbortRx();
    return;
  }
  uint16_t count = rx.length - rx.offset;
  if (count > 7U) { count = 7U; }
  if ((data[0] & 0x0FU) != rx.sequence || dlc < count + 1U) { AbortRx(); return; }
  memcpy(rx.data + rx.offset, data + 1, count);
  rx.offset += count;
  rx.sequence = (rx.sequence + 1U) & 0x0FU;
  rx.since_ms = now_ms;
  if (rx.offset == rx.length)
  {
    rx.state = RX_IDLE;
    Count(&stats.rx_complete);
    CanTp_RxComplete(rx.data, rx.length);
  }
  else if (++rx.block_count == CANTP_RX_BLOCK_SIZE && CANTP_RX_BLOCK_SIZE != 0U)
  {
    rx.block_count = 0U;
    rx.state = RX_FC;
  }
}

/* Service timeouts and send one frame; no polling loop or blocking wait is used. */
void CanTp_MainFunction(uint32_t now_ms)
{
  if (!initialized) { return; }
  PollConfirmation(now_ms);
  if (pending != FRAME_NONE) { return; }
  if (tx.state == TX_WAIT_FC && Expired(now_ms, tx.since_ms, CANTP_N_BS_MS))
  {
    Count(&stats.timeouts);
    AbortTx();
  }
  if (rx.state == RX_CF && Expired(now_ms, rx.since_ms, CANTP_N_CR_MS))
  {
    Count(&stats.timeouts);
    AbortRx();
  }

  uint8_t frame[8];
  memset(frame, CANTP_PADDING_BYTE, sizeof(frame));
  Frame_t kind = FRAME_NONE;
  uint16_t count = 0U;
  uint32_t since = tx.since_ms;
  uint32_t timeout = CANTP_N_AS_MS;
  if (rx.state == RX_FC || rx.state == RX_OVERFLOW)
  {
    kind = rx.state == RX_FC ? FRAME_FC : FRAME_OVERFLOW;
    frame[0] = kind == FRAME_FC ? 0x30U : 0x32U;
    frame[1] = kind == FRAME_FC ? CANTP_RX_BLOCK_SIZE : 0U;
    frame[2] = kind == FRAME_FC ? CANTP_RX_STMIN : 0U;
    since = rx.since_ms;
    timeout = CANTP_N_AR_MS;
  }
  else if (tx.state == TX_FIRST)
  {
    if (tx.length <= 7U)
    {
      kind = FRAME_SF;
      count = tx.length;
      frame[0] = (uint8_t)count;
      memcpy(frame + 1, tx.data, count);
    }
    else
    {
      kind = FRAME_FF;
      count = 6U;
      frame[0] = (uint8_t)(0x10U | (tx.length >> 8));
      frame[1] = (uint8_t)tx.length;
      memcpy(frame + 2, tx.data, count);
    }
  }
  else if (tx.state == TX_CF)
  {
    /* Timing from observed confirmation is conservative, including sub-ms STmin. */
    if (!Expired(now_ms, tx.confirmed_ms, tx.stmin_ms)) { return; }
    kind = FRAME_CF;
    count = tx.length - tx.offset;
    if (count > 7U) { count = 7U; }
    frame[0] = 0x20U | tx.sequence;
    memcpy(frame + 1, tx.data + tx.offset, count);
  }
  if (kind == FRAME_NONE) { return; }
  if (Expired(now_ms, since, timeout))
  {
    Count(&stats.timeouts);
    if (kind == FRAME_FC || kind == FRAME_OVERFLOW) { AbortRx(); } else { AbortTx(); }
    return;
  }
  CanTp_PortResult_t result = CanTp_PortSend(CANTP_TX_ID, frame);
  if (result == CANTP_PORT_BUSY) { return; }
  if (result == CANTP_PORT_ERROR)
  {
    if (kind == FRAME_FC || kind == FRAME_OVERFLOW) { AbortRx(); } else { AbortTx(); }
    return;
  }
  pending = kind;
  /* Mailbox retries consume the same deadline as confirmation; accepting a
   * frame late must not grant it a second full timeout interval. */
  pending_since_ms = since;
  if (kind == FRAME_SF || kind == FRAME_FF || kind == FRAME_CF) { tx.offset += count; }
  if (kind == FRAME_CF) { tx.sequence = (tx.sequence + 1U) & 0x0FU; }
}

/* Cancel both directions after the lower layer reports lost or invalid data. */
void CanTp_Cancel(void)
{
  if (tx.state != TX_IDLE) { AbortTx(); }
  if (rx.state != RX_IDLE) { AbortRx(); }
}

/* Expose read-only counters to the owning task and debugger. */
const CanTp_Stats_t *CanTp_GetStats(void)
{
  return &stats;
}
