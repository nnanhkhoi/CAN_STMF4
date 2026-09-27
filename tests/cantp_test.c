#include "CanTp.h"
#include "CanTp_Port.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t sent[512][8];
static unsigned sent_count, received_count, cancelled;
static uint8_t received[CANTP_MAX_PAYLOAD];
static uint16_t received_length;
static bool active, completed, fail_send, fail_poll, busy_send;
static unsigned tx_successes, tx_failures;

/* Observe whether accepted messages completed or failed asynchronously. */
void CanTp_TxComplete(bool success)
{
  if (success) { ++tx_successes; } else { ++tx_failures; }
}

/* Fake the CAN controller while retaining every submitted frame for assertions. */
CanTp_PortResult_t CanTp_PortSend(uint32_t id, const uint8_t data[8])
{
  assert(id == CANTP_TX_ID);
  if (fail_send) { return CANTP_PORT_ERROR; }
  if (active || busy_send) { return CANTP_PORT_BUSY; }
  assert(sent_count < 512U);
  memcpy(sent[sent_count++], data, 8U);
  active = true;
  completed = false;
  return CANTP_PORT_OK;
}

/* Complete only when a test explicitly supplies a hardware result. */
CanTp_PortResult_t CanTp_PortPoll(void)
{
  assert(active);
  if (!completed && !fail_poll) { return CANTP_PORT_BUSY; }
  active = false;
  return fail_poll ? CANTP_PORT_ERROR : CANTP_PORT_OK;
}

/* Model a cancelled mailbox and count cancellation requests. */
void CanTp_PortCancel(void)
{
  active = false;
  ++cancelled;
}

/* Copy delivered data so buffer lifetime and exact reassembly can be checked. */
void CanTp_RxComplete(uint8_t *data, uint16_t length)
{
  assert(length <= sizeof(received));
  memcpy(received, data, length);
  received_length = length;
  ++received_count;
}

/* Start each scenario with an empty controller and fresh transport statistics. */
static void Reset(void)
{
  active = completed = fail_send = fail_poll = busy_send = false;
  sent_count = received_count = received_length = 0U;
  tx_successes = tx_failures = 0U;
  CanTp_Init();
  cancelled = 0U;
}

/* Inject a frame from the configured tester. */
static void Receive(const uint8_t *frame, uint8_t dlc, uint32_t now)
{
  CanTp_RxIndication(CANTP_RX_ID, frame, dlc, now);
}

/* Acknowledge the current mailbox and run the cyclic service once. */
static void Complete(uint32_t now)
{
  completed = true;
  CanTp_MainFunction(now);
}

/* Exercise single-frame validation, ID filtering, TX buffer ownership, and BUSY. */
static void TestSingleFrames(void)
{
  uint8_t frame[8] = {2, 0x10, 3};
  Reset();
  CanTp_RxIndication(CANTP_RX_ID + 1U, frame, 8U, 0U);
  assert(received_count == 0U);
  Receive(frame, 8U, 0U);
  assert(received_count == 1U && received_length == 2U);
  assert(received[0] == 0x10U && received[1] == 3U);
  Receive(frame, 2U, 0U);
  frame[0] = 0U;
  Receive(frame, 8U, 0U);
  frame[0] = 8U;
  Receive(frame, 8U, 0U);
  Receive(frame, 9U, 0U);
  assert(CanTp_GetStats()->rx_errors == 4U);
  assert(CanTp_Transmit(NULL, 1U, 0U) == CANTP_INVALID);
  assert(CanTp_Transmit(frame, 0U, 0U) == CANTP_INVALID);
  assert(CanTp_Transmit(frame, CANTP_MAX_PAYLOAD + 1U, 0U) == CANTP_INVALID);
  frame[0] = 0x62U;
  assert(CanTp_Transmit(frame, 7U, 0U) == CANTP_OK);
  frame[0] = 0U;
  assert(CanTp_Transmit(frame, 1U, 0U) == CANTP_BUSY);
  CanTp_MainFunction(0U);
  assert(sent_count == 1U && sent[0][0] == 7U && sent[0][1] == 0x62U);
  CanTp_MainFunction(10U);
  assert(CanTp_GetStats()->tx_complete == 0U);
  Complete(11U);
  assert(CanTp_GetStats()->tx_complete == 1U);
  assert(tx_successes == 1U && tx_failures == 0U);
}

/* Reassemble a long payload across FC blocks and the sequence-number wrap. */
static void TestReceiveLong(void)
{
  uint8_t expected[CANTP_MAX_PAYLOAD];
  uint8_t frame[8] = {0};
  uint32_t now = 0U;
  unsigned block = 0U;
  for (unsigned i = 0; i < sizeof(expected); ++i) { expected[i] = (uint8_t)i; }
  Reset();
  frame[0] = (uint8_t)(0x10U | (sizeof(expected) >> 8));
  frame[1] = (uint8_t)sizeof(expected);
  memcpy(frame + 2, expected, 6U);
  Receive(frame, 8U, now);
  uint16_t offset = 6U;
  uint8_t sn = 1U;
  while (offset < sizeof(expected))
  {
    if (block == 0U)
    {
      CanTp_MainFunction(now);
      assert(sent[sent_count - 1U][0] == 0x30U);
      assert(sent[sent_count - 1U][1] == CANTP_RX_BLOCK_SIZE);
      assert(sent[sent_count - 1U][2] == CANTP_RX_STMIN);
      Complete(++now);
    }
    unsigned count = sizeof(expected) - offset;
    if (count > 7U) { count = 7U; }
    frame[0] = 0x20U | sn;
    memcpy(frame + 1, expected + offset, count);
    now += 10U;
    Receive(frame, (uint8_t)(count + 1U), now);
    offset += (uint16_t)count;
    sn = (sn + 1U) & 15U;
    block = (block + 1U) % CANTP_RX_BLOCK_SIZE;
  }
  assert(received_count == 1U && received_length == sizeof(expected));
  assert(memcmp(received, expected, sizeof(expected)) == 0);
  assert(CanTp_GetStats()->rx_errors == 0U);
}

/* Verify frame boundaries, BS=0/1/4, final padding, and successful final ACK. */
static void TestTransmitLong(uint16_t length, uint8_t bs)
{
  uint8_t payload[CANTP_MAX_PAYLOAD];
  uint8_t fc[8] = {0x30, bs, 0};
  uint32_t now = 0U;
  for (unsigned i = 0; i < length; ++i) { payload[i] = (uint8_t)(i * 3U); }
  Reset();
  assert(CanTp_Transmit(payload, length, now) == CANTP_OK);
  CanTp_MainFunction(now);
  assert(sent[0][0] == (0x10U | (length >> 8)) && sent[0][1] == (uint8_t)length);
  assert(memcmp(sent[0] + 2, payload, 6U) == 0);
  Complete(++now);
  assert(sent_count == 1U);
  uint16_t offset = 6U;
  uint8_t sn = 1U;
  unsigned block = 0U;
  while (offset < length)
  {
    if (block == 0U)
    {
      Receive(fc, 3U, now);
      CanTp_MainFunction(now);
    }
    assert(active);
    unsigned count = length - offset;
    if (count > 7U) { count = 7U; }
    const uint8_t *frame = sent[sent_count - 1U];
    assert(frame[0] == (0x20U | sn));
    assert(memcmp(frame + 1, payload + offset, count) == 0);
    for (unsigned i = count + 1U; i < 8U; ++i) { assert(frame[i] == CANTP_PADDING_BYTE); }
    offset += (uint16_t)count;
    sn = (sn + 1U) & 15U;
    ++block;
    Complete(++now);
    if (bs != 0U && block == bs) { block = 0U; }
  }
  assert(!active && CanTp_GetStats()->tx_complete == 1U);
  assert(CanTp_GetStats()->tx_errors == 0U);
}

/* Ensure STmin starts after observed confirmation, including reserved values. */
static void TestSeparation(uint8_t encoded, uint32_t milliseconds)
{
  uint8_t payload[21] = {0};
  uint8_t fc[3] = {0x30, 0, encoded};
  Reset();
  assert(CanTp_Transmit(payload, sizeof(payload), 0U) == CANTP_OK);
  CanTp_MainFunction(0U);
  Complete(10U);
  Receive(fc, sizeof(fc), 10U);
  CanTp_MainFunction(10U + milliseconds - 1U);
  assert(sent_count == 1U);
  CanTp_MainFunction(10U + milliseconds);
  assert(sent_count == 2U);
  Complete(20U + milliseconds);
  assert(sent_count == 2U);
  CanTp_MainFunction(20U + 2U * milliseconds);
  assert(sent_count == 3U);
}

/* Inject malformed traffic, peer refusal, timeouts, cancellation and CAN faults. */
static void TestFailures(void)
{
  uint8_t payload[20] = {0};
  uint8_t ff[8] = {0x10, 20, 1, 2, 3, 4, 5, 6};
  uint8_t cf[8] = {0x22, 7, 8, 9, 10, 11, 12, 13};
  uint8_t fc[3] = {0x31, 0, 0};
  Reset();
  Receive(ff, 8U, 0U);
  CanTp_MainFunction(0U);
  Complete(1U);
  Receive(cf, 8U, 2U); /* Wrong sequence; callback must never receive partial data. */
  assert(received_count == 0U && CanTp_GetStats()->rx_errors == 1U);
  Reset();
  Receive(ff, 8U, 0U);
  cf[0] = 0x21U;
  Receive(cf, 8U, 1U); /* CF before CTS confirmation. */
  assert(CanTp_GetStats()->rx_errors == 1U);
  Reset();
  Receive(ff, 7U, 0U); /* FF must carry six initial data bytes. */
  assert(CanTp_GetStats()->rx_errors == 1U);
  ff[0] = 0x1FU; ff[1] = 0xFFU;
  Receive(ff, 8U, 1U);
  CanTp_MainFunction(1U);
  assert(sent[0][0] == 0x32U); /* Receiver buffer overflow. */
  Complete(2U);
  ff[0] = 0x10U; ff[1] = 20U;

  Reset();
  assert(CanTp_Transmit(payload, sizeof(payload), 0U) == CANTP_OK);
  CanTp_MainFunction(0U); Complete(1U);
  for (unsigned i = 0; i < CANTP_MAX_WAIT_FRAMES; ++i) { Receive(fc, 3U, 2U + i); }
  assert(CanTp_GetStats()->tx_errors == 0U);
  Receive(fc, 3U, 10U);
  assert(CanTp_GetStats()->tx_errors == 1U);
  const uint8_t invalid_fc[] = {0x32, 0x34, 0x30};
  for (unsigned i = 0; i < sizeof(invalid_fc); ++i)
  {
    Reset();
    assert(CanTp_Transmit(payload, sizeof(payload), 0U) == CANTP_OK);
    CanTp_MainFunction(0U); Complete(1U);
    fc[0] = invalid_fc[i];
    Receive(fc, i == 2U ? 2U : 3U, 2U);
    assert(CanTp_GetStats()->tx_errors == 1U);
  }
  Reset();
  assert(CanTp_Transmit(payload, sizeof(payload), UINT32_MAX - 10U) == CANTP_OK);
  CanTp_MainFunction(UINT32_MAX - 10U); Complete(UINT32_MAX - 9U);
  CanTp_MainFunction(CANTP_N_BS_MS - 10U); /* Tick wrap during N_Bs. */
  assert(CanTp_GetStats()->timeouts == 1U && CanTp_GetStats()->tx_errors == 1U);
  Reset();
  Receive(ff, 8U, 0U); CanTp_MainFunction(0U); Complete(1U);
  CanTp_MainFunction(CANTP_N_CR_MS + 1U);
  assert(CanTp_GetStats()->timeouts == 1U && received_count == 0U);
  for (unsigned busy = 0; busy < 2U; ++busy)
  {
    Reset(); busy_send = busy != 0U;
    assert(CanTp_Transmit(payload, 1U, 0U) == CANTP_OK);
    CanTp_MainFunction(0U); CanTp_MainFunction(CANTP_N_AS_MS);
    assert(CanTp_GetStats()->timeouts == 1U && CanTp_GetStats()->tx_errors == 1U);
    Reset(); busy_send = busy != 0U;
    Receive(ff, 8U, 0U); CanTp_MainFunction(0U); CanTp_MainFunction(CANTP_N_AR_MS);
    assert(CanTp_GetStats()->timeouts == 1U && CanTp_GetStats()->rx_errors == 1U);
  }
  Reset(); fail_send = true;
  assert(CanTp_Transmit(payload, 1U, 0U) == CANTP_OK);
  CanTp_MainFunction(0U);
  assert(CanTp_GetStats()->tx_errors == 1U);
  Reset(); busy_send = true;
  assert(CanTp_Transmit(payload, 1U, 0U) == CANTP_OK);
  CanTp_MainFunction(0U);
  busy_send = false;
  CanTp_MainFunction(CANTP_N_AS_MS - 1U);
  assert(active);
  CanTp_MainFunction(CANTP_N_AS_MS);
  assert(CanTp_GetStats()->timeouts == 1U && CanTp_GetStats()->tx_errors == 1U);
  Reset();
  assert(CanTp_Transmit(payload, 1U, 0U) == CANTP_OK);
  CanTp_MainFunction(0U); fail_poll = true; CanTp_MainFunction(1U);
  assert(CanTp_GetStats()->tx_errors == 1U && CanTp_GetStats()->tx_complete == 0U);
  Reset();
  Receive(ff, 8U, 0U); CanTp_MainFunction(0U); CanTp_Cancel();
  assert(cancelled != 0U && received_count == 0U);
}

/* Run deterministic protocol tests against the real portable CanTp state machine. */
int main(void)
{
  TestSingleFrames();
  TestReceiveLong();
  const uint16_t lengths[] = {8U, 13U, 14U, 120U, 256U, CANTP_MAX_PAYLOAD};
  const uint8_t blocks[] = {0U, 1U, 4U};
  for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
  {
    for (unsigned j = 0; j < sizeof(blocks); ++j) { TestTransmitLong(lengths[i], blocks[j]); }
  }
  TestSeparation(20U, 20U);
  TestSeparation(0xF1U, 1U);
  TestSeparation(0x80U, 127U);
  TestFailures();
  puts("CanTp protocol tests passed");
  return 0;
}
