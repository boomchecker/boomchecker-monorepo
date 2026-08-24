/**
 ******************************************************************************
 * @file    link_tx_test.c
 * @brief   The link engine's TX pipeline: boomlink.md section 9.1's dequeue and
 *          sequence assignment, 9.6's stop-and-wait retry, and both of 9.7's
 *          randomized delays - the TX jitter before a first transmission and the
 *          backoff before a retry.
 *
 *          Section 15.2 lists ACK matching, ACK timeout, retry count, queue
 *          priority, queue overflow and randomized backoff bounds as things a
 *          fake radio backend has to test, and it is right that none of them is
 *          testable otherwise: against real hardware a timeout is a sleep, a
 *          retry count depends on whether a packet happened to be lost, and the
 *          backoff bounds are unobservable.
 *
 *          Two deliberate choices about HOW that is done here:
 *
 *          Timeouts and backoffs are MEASURED, not recomputed. A test that
 *          derived the expected ACK window from the same three terms the engine
 *          uses would agree with the engine about a wrong formula - and it would
 *          keep agreeing when the formula changed. So the scenarios advance the
 *          clock one millisecond at a time and observe when the pipeline moves,
 *          then assert the PROPERTIES section 9.6 actually requires: that the
 *          window is longer than the fixed margin (so airtime contributes at
 *          all), and that a longer frame gets a longer window (so it is derived
 *          from the profile rather than fixed).
 *
 *          Retries are provoked by SILENCE, not by a swallowed transmission. A
 *          frame nobody answers is what a lost ACK looks like from the sender's
 *          side, and it leaves every attempt on the air where the test can read
 *          it - which is how "the retransmission reuses the same (session_id,
 *          sequence)" can be checked against the bytes rather than against a
 *          counter.
 ******************************************************************************
 */
#include <string.h>

#include "boomlink_link.h"
#include "c_test.h"
#include "fake_port.h"

BOOMLINK_TEST_STATE;

#define NODE_A 0x00000011u
#define NODE_B 0x00000022u
#define NODE_C 0x00000033u

/* Explicit rather than inherited from a default, because three scenarios reason
   about them: the margin is what the measured ACK window must exceed, and the
   backoff bounds are what the measured delays must fall inside. */
#define MARGIN_MS      50u
#define BACKOFF_MIN_MS 20u
#define BACKOFF_MAX_MS 60u

/* An upper bound for the measuring loops - generous enough that hitting it means
   the pipeline is stuck, not that the timeout was long. */
#define MEASURE_LIMIT_MS 5000u

typedef struct {
  unsigned              calls;
  boomlink_tx_outcome_t last_outcome;
  uint32_t              last_destination;
  uint32_t              last_sequence;
  uint8_t               last_attempts;
  float                 last_rssi_dbm;
  float                 last_snr_db;
  unsigned              acked;
  unsigned              sent;
  unsigned              no_ack;
} tx_log_t;

static void tx_capture(void *user, boomlink_tx_outcome_t outcome, uint32_t destination_id,
                       uint32_t sequence, uint8_t attempts, float rssi_dbm, float snr_db) {
  tx_log_t *log         = (tx_log_t *)user;
  log->calls++;
  log->last_outcome     = outcome;
  log->last_destination = destination_id;
  log->last_sequence    = sequence;
  log->last_attempts    = attempts;
  log->last_rssi_dbm    = rssi_dbm;
  log->last_snr_db      = snr_db;
  switch (outcome) {
    case BOOMLINK_TX_ACKED:
      log->acked++;
      break;
    case BOOMLINK_TX_SENT:
      log->sent++;
      break;
    case BOOMLINK_TX_NO_ACK:
      log->no_ack++;
      break;
  }
}

typedef struct {
  unsigned calls;
  uint32_t last_source;
  size_t   last_len;
  uint8_t  last_payload[8];
} rx_log_t;

static void rx_capture(void *user, uint32_t source_id, uint32_t destination_id,
                       const uint8_t *payload, size_t payload_len, float rssi_dbm,
                       float snr_db) {
  rx_log_t *log    = (rx_log_t *)user;
  (void)destination_id;
  (void)rssi_dbm;
  (void)snr_db;
  log->calls++;
  log->last_source = source_id;
  log->last_len    = payload_len;
  if (payload_len > sizeof(log->last_payload)) {
    payload_len = sizeof(log->last_payload);
  }
  memcpy(log->last_payload, payload, payload_len);
}

typedef struct {
  boomlink_link_t link;
  boomlink_port_t port;
  fake_port_ctx_t ctx;
  tx_log_t        tx;
  rx_log_t        rx;
} node_t;

static bool node_up_jittered(node_t *n, fake_air_t *air, uint8_t index, uint32_t node_id,
                             uint8_t max_attempts, size_t max_packet,
                             uint32_t tx_jitter_max_ms) {
  memset(n, 0, sizeof(*n));
  fake_port_init(&n->ctx, air, index, 0x1000u + node_id, &n->port);
  if (max_packet != 0u) {
    n->port.max_packet = max_packet;
  }
  boomlink_link_config_t cfg = {
      .node_id               = node_id,
      .magic                 = BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
      .ack_timeout_margin_ms = MARGIN_MS,
      .max_attempts          = max_attempts,
      .backoff_min_ms        = BACKOFF_MIN_MS,
      .backoff_max_ms        = BACKOFF_MAX_MS,
      /* Zero in every scenario but the jitter one. Not laziness: section 9.7's
         jitter delays the FIRST transmission, so with it on, "poll once and the
         frame is on the air" stops holding and every other scenario here would
         have to advance a clock before asserting anything. Off by default keeps
         each scenario about the one mechanism it names. */
      .tx_jitter_max_ms      = tx_jitter_max_ms,
      .on_rx                 = rx_capture,
      .on_rx_user            = &n->rx,
      .on_tx_done            = tx_capture,
      .on_tx_done_user       = &n->tx,
  };
  return boomlink_link_init(&n->link, &cfg, &n->port, 0x5000u + node_id);
}

static bool node_up_full(node_t *n, fake_air_t *air, uint8_t index, uint32_t node_id,
                         uint8_t max_attempts, size_t max_packet) {
  return node_up_jittered(n, air, index, node_id, max_attempts, max_packet, 0u);
}

static bool node_up(node_t *n, fake_air_t *air, uint8_t index, uint32_t node_id) {
  return node_up_full(n, air, index, node_id, 3u, 0u);
}

static void scenario_end(const fake_air_t *air) {
  CHECK(fake_air_ok(air),
        "the transmission log overflowed: this scenario's assertions were made "
        "against a truncated prefix of what was actually sent");
}

/**
 * Advance `n`'s clock one millisecond at a time, polling after each step, until
 * the TX pipeline leaves `from`. Returns how many milliseconds that took, or 0 if
 * it never happened.
 *
 * One millisecond at a time so the answer is the real threshold rather than
 * whatever step size the test happened to pick: a scenario that jumped straight
 * to the value it expected could not tell a correct window from one twice as
 * long. It also means the "not yet" side is checked implicitly on every
 * iteration, since a pipeline that moved early returns a smaller number.
 */
static uint32_t ms_until_state_leaves(node_t *n, boomlink_tx_state_t from) {
  for (uint32_t ms = 1u; ms <= MEASURE_LIMIT_MS; ms++) {
    fake_port_advance_ms(&n->ctx, 1u);
    boomlink_link_poll(&n->link);
    if (boomlink_link_tx_state(&n->link) != from) {
      return ms;
    }
  }
  return 0u;
}

/** Parse the `index`-th transmission on the air. */
static bool parse_air(const fake_air_t *air, size_t index,
                      boomlink_linkframe_header_t *out_header, size_t *out_payload_len) {
  const fake_transmission_t *tx = fake_air_transmission(air, index);
  if (tx == NULL) {
    return false;
  }
  return boomlink_linkframe_parse(tx->bytes, tx->len, BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
                                  out_header, out_payload_len) == BOOMLINK_LINKFRAME_OK;
}

static void test_an_ack_completes_the_pending_frame(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a, b;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  const uint8_t payload[] = {0xC7};
  REQUIRE(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, true, payload,
                             sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
          "queued");
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_IDLE,
        "send() does not transmit - section 9.1 assigns the sequence at dequeue");
  CHECK(boomlink_link_queue_depth(&a.link) == 1u, "it is in the queue");

  boomlink_link_poll(&a.link);
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_WAIT_ACK,
        "after transmission the pipeline waits for the ACK");
  CHECK(boomlink_link_queue_depth(&a.link) == 0u, "and the frame left the queue");
  CHECK(a.tx.calls == 0u, "nothing is finished yet");

  boomlink_link_poll(&b.link); /* B receives and acknowledges */
  REQUIRE(fake_air_count(&air) == 2u, "the DATA frame and its ACK, got %zu",
          fake_air_count(&air));
  boomlink_link_poll(&a.link); /* A hears the ACK */

  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_IDLE,
        "the acknowledged frame left the pipeline");
  boomlink_link_stats_t as;
  boomlink_link_get_stats(&a.link, &as);
  CHECK(as.ack_received == 1u, "the ACK was counted as received, got %u",
        (unsigned)as.ack_received);
  CHECK(as.ack_unmatched == 0u, "and not as unmatched");
  CHECK(as.tx_envelopes == 1u, "one envelope");
  CHECK(as.tx_retries == 0u, "no retries were needed");
  CHECK(as.tx_failures == 0u, "and no failures");

  /* Section 9.6's "final failure is surfaced to the caller" - reported for
     success too, or a caller could not tell delivery from a frame still queued
     behind a retry sequence. */
  REQUIRE(a.tx.calls == 1u, "the outcome was reported exactly once, got %u", a.tx.calls);
  CHECK(a.tx.last_outcome == BOOMLINK_TX_ACKED, "as acknowledged");
  CHECK(a.tx.last_destination == NODE_B, "for the right destination");
  CHECK(a.tx.last_attempts == 1u, "after one attempt, got %u",
        (unsigned)a.tx.last_attempts);
  /* The ACKing frame's OWN signal quality, not a placeholder - a BOOMLINK_TX_ACKED
     outcome is a received packet, and section 9.10's whole reason last_rssi_dbm/
     last_snr_db exist is that this is worth reporting. */
  CHECK(a.tx.last_rssi_dbm < 0.0f, "the ACK's RSSI was reported, got %f",
        (double)a.tx.last_rssi_dbm);
  CHECK(a.tx.last_snr_db > 0.0f, "and its SNR, got %f", (double)a.tx.last_snr_db);

  boomlink_linkframe_header_t sent;
  size_t                      sent_payload = 0u;
  REQUIRE(parse_air(&air, 0u, &sent, &sent_payload), "the DATA frame parses");
  CHECK(a.tx.last_sequence == sent.sequence,
        "and the reported sequence is the one that went on the air: %u vs %u",
        (unsigned)a.tx.last_sequence, (unsigned)sent.sequence);

  /* Time passing must not resurrect a finished frame. */
  fake_port_advance_ms(&a.ctx, 10000u);
  boomlink_link_poll(&a.link);
  CHECK(fake_air_count(&air) == 2u, "no retransmission of an acknowledged frame");
  scenario_end(&air);
}

static void test_an_unanswered_frame_is_retried_with_the_same_sequence(void) {
  fake_air_t air;
  fake_air_init(&air);
  /* Deliberately ONE node. Nobody answers, which is exactly what a lost ACK looks
     like to the sender - and every attempt stays on the air where its bytes can be
     compared, which a swallowed transmission would not allow. */
  node_t a;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");

  const uint8_t payload[] = {0x01, 0x02, 0x03};
  REQUIRE(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, true, payload,
                             sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
          "queued");
  boomlink_link_poll(&a.link);
  REQUIRE(fake_air_count(&air) == 1u, "the first attempt is on the air");

  /* max_attempts is 3: attempt, timeout, backoff, attempt, timeout, backoff,
     attempt, timeout, give up. Section 9.6's "default target: 3 total
     transmission attempts" - total, not retries after the first. */
  for (unsigned round = 0; round < 2u; round++) {
    const uint32_t window = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_WAIT_ACK);
    CHECK(window > MARGIN_MS,
          "round %u: the ACK window (%u ms) must exceed the fixed margin (%u ms), or "
          "the airtime term contributes nothing and section 9.6's "
          "profile-derived timeout is decoration",
          round, (unsigned)window, (unsigned)MARGIN_MS);
    CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_BACKOFF,
          "round %u: a timeout goes to backoff, not straight to the retry - section "
          "9.7 requires jitter on the retry too",
          round);
    CHECK(fake_air_count(&air) == round + 1u,
          "round %u: nothing was transmitted during the wait or the timeout", round);

    const uint32_t backoff = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_BACKOFF);
    CHECK(backoff >= BACKOFF_MIN_MS && backoff <= BACKOFF_MAX_MS,
          "round %u: the backoff was %u ms, outside the configured [%u, %u]", round,
          (unsigned)backoff, (unsigned)BACKOFF_MIN_MS, (unsigned)BACKOFF_MAX_MS);
    CHECK(fake_air_count(&air) == round + 2u,
          "round %u: the retransmission went out when the backoff expired, %zu on air",
          round, fake_air_count(&air));
  }

  REQUIRE(fake_air_count(&air) == 3u, "three attempts, got %zu", fake_air_count(&air));

  /* The bytes, not the counters. Section 9.6: "retransmission uses the SAME
     (session_id, sequence) so the receiver can suppress duplicate delivery" - a
     retry that re-assigned the sequence would be delivered to the application
     twice, and the sender would never know. */
  const fake_transmission_t *first = fake_air_transmission(&air, 0u);
  REQUIRE(first != NULL, "the first attempt was logged");
  for (size_t i = 1u; i < 3u; i++) {
    const fake_transmission_t *again = fake_air_transmission(&air, i);
    REQUIRE(again != NULL, "attempt %zu was logged", i);
    CHECK(again->len == first->len, "attempt %zu is the same length", i);
    CHECK(memcmp(again->bytes, first->bytes, first->len) == 0,
          "attempt %zu differs from the first byte for byte - a retransmission must "
          "be identical, sequence included",
          i);
  }

  boomlink_link_stats_t as;
  boomlink_link_get_stats(&a.link, &as);
  CHECK(as.tx_envelopes == 1u, "one envelope, whatever the attempt count, got %u",
        (unsigned)as.tx_envelopes);
  CHECK(as.tx_retries == 2u, "two retries, got %u", (unsigned)as.tx_retries);

  /* And it stops. Section 9.6: "do not retry forever". */
  const uint32_t last_window = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_WAIT_ACK);
  CHECK(last_window > MARGIN_MS, "the third attempt got its own window too");
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_IDLE,
        "after the last permitted attempt the pipeline is released, not backed off");
  CHECK(fake_air_count(&air) == 3u, "and no fourth attempt was made, %zu on air",
        fake_air_count(&air));

  boomlink_link_get_stats(&a.link, &as);
  CHECK(as.tx_failures == 1u, "the final failure was counted, got %u",
        (unsigned)as.tx_failures);
  REQUIRE(a.tx.calls == 1u, "and reported once, got %u", a.tx.calls);
  CHECK(a.tx.last_outcome == BOOMLINK_TX_NO_ACK, "as a failure, not a success");
  CHECK(a.tx.last_attempts == 3u, "after three attempts, got %u",
        (unsigned)a.tx.last_attempts);
  CHECK(a.tx.last_destination == NODE_B, "for the destination that never answered");

  /* Nothing more happens, ever. */
  fake_port_advance_ms(&a.ctx, 100000u);
  boomlink_link_poll(&a.link);
  boomlink_link_poll(&a.link);
  CHECK(fake_air_count(&air) == 3u, "a given-up frame is not resurrected by time");
  scenario_end(&air);
}

static void test_the_queue_is_held_while_a_frame_awaits_its_ack(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a;
  REQUIRE(node_up_full(&a, &air, 0u, NODE_A, 1u, 0u), "A came up with one attempt");

  /* Section 9.6: "at most one ACK-pending frame is outstanding at any time,
     globally. While waiting for an ACK the TX queue is held - the radio is
     half-duplex, and transmitting another frame during the ACK wait window would
     prevent hearing the ACK. Frames that do not request ACK simply wait in the
     queue behind the pending one."
     Both frames are HIGH, so the queue's own priority ordering cannot explain
     anything here: within one priority the order is arrival order (section 9.8),
     the first frame is the one that goes out, and the second is as urgent as
     traffic gets. The second also asks for no ACK, so it needs nothing from the
     radio but the airtime. If it still waits, the only thing holding it is the
     stop-and-wait rule. */
  const uint8_t first[]  = {0xF1};
  const uint8_t second[] = {0xF2};
  REQUIRE(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_HIGH, true, first,
                             sizeof(first)) == BOOMLINK_LINK_SEND_OK,
          "the ACK-requesting frame is queued");
  REQUIRE(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_HIGH, false, second,
                             sizeof(second)) == BOOMLINK_LINK_SEND_OK,
          "and an equally urgent one behind it");

  boomlink_link_poll(&a.link);
  REQUIRE(fake_air_count(&air) == 1u, "one frame went out");
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_WAIT_ACK, "and waits");
  CHECK(boomlink_link_queue_depth(&a.link) == 1u, "the urgent frame is still queued");

  /* Polling repeatedly must not release it: the ACK window has not expired. */
  for (int i = 0; i < 10; i++) {
    boomlink_link_poll(&a.link);
  }
  CHECK(fake_air_count(&air) == 1u,
        "the queue must stay held while an ACK is pending, %zu on air",
        fake_air_count(&air));
  CHECK(boomlink_link_queue_depth(&a.link) == 1u, "still queued");

  /* One attempt only, so the timeout ends it outright and the queue moves. */
  const uint32_t window = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_WAIT_ACK);
  CHECK(window > MARGIN_MS, "the window was measured");
  CHECK(a.tx.no_ack == 1u, "the first frame gave up, got %u", a.tx.no_ack);
  boomlink_link_poll(&a.link);
  REQUIRE(fake_air_count(&air) == 2u, "the held frame went out next, %zu on air",
          fake_air_count(&air));

  boomlink_linkframe_header_t h0, h1;
  size_t                      p0 = 0u, p1 = 0u;
  REQUIRE(parse_air(&air, 0u, &h0, &p0) && parse_air(&air, 1u, &h1, &p1), "both parse");
  const fake_transmission_t *tx1 = fake_air_transmission(&air, 1u);
  REQUIRE(tx1 != NULL, "logged");
  CHECK(tx1->bytes[BOOMLINK_LINKFRAME_HEADER_SIZE] == second[0],
        "and it is the second payload, got %#04x",
        tx1->bytes[BOOMLINK_LINKFRAME_HEADER_SIZE]);
  CHECK(h1.sequence == h0.sequence + 1u,
        "with the next sequence: %u after %u - the held frame's sequence is assigned "
        "when it finally leaves the queue, not when it was queued",
        (unsigned)h1.sequence, (unsigned)h0.sequence);
  scenario_end(&air);
}

static void test_a_refused_send_keeps_the_frame_and_its_order(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");

  /* Two DIFFERENT payloads, which is the whole point. An earlier version of this
     engine popped the frame off the queue BEFORE transmitting and dropped it when
     the radio refused, so a busy radio silently destroyed queued traffic - and a
     scenario sending the same bytes twice could not see it, because the survivor
     was indistinguishable from the victim. */
  const uint8_t one[] = {0xA1};
  const uint8_t two[] = {0xB2};
  a.ctx.send_result   = -1; /* busy or absent, as radio.h reports it */
  REQUIRE(boomlink_link_send(&a.link, NODE_A + 1u, BOOMLINK_TXPRIO_NORMAL, false, one,
                             sizeof(one)) == BOOMLINK_LINK_SEND_OK,
          "first queued");
  REQUIRE(boomlink_link_send(&a.link, NODE_A + 1u, BOOMLINK_TXPRIO_NORMAL, false, two,
                             sizeof(two)) == BOOMLINK_LINK_SEND_OK,
          "second queued");

  for (int i = 0; i < 5; i++) {
    boomlink_link_poll(&a.link);
  }
  boomlink_link_stats_t as;
  boomlink_link_get_stats(&a.link, &as);
  CHECK(fake_air_count(&air) == 0u, "nothing reached the air");
  CHECK(as.tx_envelopes == 0u, "and nothing was counted as sent");
  CHECK(as.tx_airtime_us == 0u, "a refused send radiates nothing");
  CHECK(as.tx_failures == 5u, "every refusal is visible, got %u",
        (unsigned)as.tx_failures);
  CHECK(as.tx_retries == 0u,
        "a refused send is not a retry: nothing was radiated to retry, and section "
        "9.6's attempt budget must not be spent on it");
  CHECK(a.tx.calls == 0u, "and no frame was reported as finished - none is lost");
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_READY,
        "the first frame is still held, ready to try again");
  CHECK(boomlink_link_queue_depth(&a.link) == 1u, "with the second still queued");

  /* The radio recovers. Both frames must go out, in the order they were queued. */
  a.ctx.send_result = 0;
  boomlink_link_poll(&a.link);
  boomlink_link_poll(&a.link);
  REQUIRE(fake_air_count(&air) == 2u, "both frames went out, %zu on air",
          fake_air_count(&air));

  const fake_transmission_t *t0 = fake_air_transmission(&air, 0u);
  const fake_transmission_t *t1 = fake_air_transmission(&air, 1u);
  REQUIRE(t0 != NULL && t1 != NULL, "both logged");
  CHECK(t0->bytes[BOOMLINK_LINKFRAME_HEADER_SIZE] == one[0],
        "the frame the radio refused is the one that went out first, got %#04x",
        t0->bytes[BOOMLINK_LINKFRAME_HEADER_SIZE]);
  CHECK(t1->bytes[BOOMLINK_LINKFRAME_HEADER_SIZE] == two[0],
        "then the one queued behind it, got %#04x",
        t1->bytes[BOOMLINK_LINKFRAME_HEADER_SIZE]);

  boomlink_linkframe_header_t h0, h1;
  size_t                      p0 = 0u, p1 = 0u;
  REQUIRE(parse_air(&air, 0u, &h0, &p0) && parse_air(&air, 1u, &h1, &p1), "both parse");
  CHECK(h1.sequence == h0.sequence + 1u,
        "and the sequence advanced by exactly one across the refusals: %u then %u - "
        "a sequence consumed per refused attempt would leave gaps",
        (unsigned)h0.sequence, (unsigned)h1.sequence);
  boomlink_link_get_stats(&a.link, &as);
  CHECK(as.tx_envelopes == 2u, "both counted once, got %u", (unsigned)as.tx_envelopes);
  CHECK(a.tx.sent == 2u, "and both reported as sent, got %u", a.tx.sent);
  scenario_end(&air);
}

static void test_a_near_miss_ack_leaves_the_frame_pending(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");

  const uint8_t payload[] = {0x33};
  REQUIRE(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, true, payload,
                             sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
          "queued");
  boomlink_link_poll(&a.link);
  boomlink_linkframe_header_t sent;
  size_t                      sent_payload = 0u;
  REQUIRE(parse_air(&air, 0u, &sent, &sent_payload), "the DATA frame is on the air");
  REQUIRE(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_WAIT_ACK, "and waits");

  /* The real ACK, then one field changed at a time. This is the case no delivery
     test can reach: an over-permissive matcher - one comparing only (session_id,
     sequence) - accepts every one of these, and every delivery test still passes
     because a correct ACK matches too. What breaks is only rejection. */
  boomlink_linkframe_header_t good;
  REQUIRE(boomlink_linkframe_make_ack(&sent, NODE_B, &good), "the ACK can be built");

  struct {
    const char                 *what;
    boomlink_linkframe_header_t ack;
  } cases[4];
  cases[0].what                = "from the wrong node";
  cases[0].ack                 = good;
  cases[0].ack.source_id       = NODE_C;
  cases[1].what                = "for the wrong sequence";
  cases[1].ack                 = good;
  cases[1].ack.sequence        = sent.sequence + 1u;
  cases[2].what                = "for the wrong session";
  cases[2].ack                 = good;
  cases[2].ack.session_id      = sent.session_id ^ 0xFFu;
  cases[3].what                = "as a DATA frame rather than an ACK";
  cases[3].ack                 = good;
  cases[3].ack.frame_type      = BOOMLINK_FRAME_TYPE_DATA;

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    uint8_t bytes[BOOMLINK_LINKFRAME_HEADER_SIZE];
    boomlink_linkframe_encode(&cases[i].ack, bytes);
    fake_air_inject(&air, 1u, bytes, sizeof(bytes));
    boomlink_link_poll(&a.link);
    CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_WAIT_ACK,
          "an ACK %s must not complete the pending frame", cases[i].what);
    CHECK(a.tx.calls == 0u, "nor report it finished (%s)", cases[i].what);
  }

  boomlink_link_stats_t as;
  boomlink_link_get_stats(&a.link, &as);
  CHECK(as.ack_received == 0u, "none of the near misses was accepted, got %u",
        (unsigned)as.ack_received);
  /* Three of the four are ACKs addressed to us that match nothing. The fourth is
     a DATA frame, so it goes down the data path instead - from a source that is a
     real peer, addressed to us, and it lands as a delivered envelope. Counted
     here so the totals below are exact rather than approximate. */
  CHECK(as.ack_unmatched == 3u, "the three unmatched ACKs were counted, got %u",
        (unsigned)as.ack_unmatched);
  CHECK(as.rx_envelopes == 1u,
        "and the one that was not an ACK went down the DATA path, got %u",
        (unsigned)as.rx_envelopes);

  /* The control: the real ACK completes it, so the matcher is not simply
     rejecting everything. */
  uint8_t bytes[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&good, bytes);
  fake_air_inject(&air, 1u, bytes, sizeof(bytes));
  boomlink_link_poll(&a.link);
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_IDLE,
        "the correct ACK completes the frame");
  CHECK(a.tx.acked == 1u, "and reports it acknowledged, got %u", a.tx.acked);
  boomlink_link_get_stats(&a.link, &as);
  CHECK(as.ack_received == 1u, "counted once, got %u", (unsigned)as.ack_received);
  CHECK(fake_air_count(&air) == 6u, "no retransmission happened, %zu on air",
        fake_air_count(&air));
  scenario_end(&air);
}

static void test_the_backoff_is_randomized_within_its_bounds(void) {
  fake_air_t air;
  fake_air_init(&air);
  /* Eight attempts, so one unanswered frame yields seven backoff draws - enough
     to say something about the distribution rather than about one number. */
  node_t a;
  REQUIRE(node_up_full(&a, &air, 0u, NODE_A, 8u, 0u), "A came up with eight attempts");

  const uint8_t payload[] = {0x9Cu};
  REQUIRE(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, true, payload,
                             sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
          "queued");
  boomlink_link_poll(&a.link);

  uint32_t draws[7];
  size_t   count = 0u;
  while (count < sizeof(draws) / sizeof(draws[0])) {
    const uint32_t window = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_WAIT_ACK);
    REQUIRE(window > 0u, "draw %zu: the ACK window expired", count);
    REQUIRE(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_BACKOFF,
            "draw %zu: and backoff started", count);
    draws[count] = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_BACKOFF);
    REQUIRE(draws[count] > 0u, "draw %zu: the backoff expired", count);
    count++;
  }

  bool all_in_range = true;
  bool any_distinct = false;
  for (size_t i = 0; i < count; i++) {
    if (draws[i] < BACKOFF_MIN_MS || draws[i] > BACKOFF_MAX_MS) {
      all_in_range = false;
      CHECK(false, "draw %zu was %u ms, outside [%u, %u]", i, (unsigned)draws[i],
            (unsigned)BACKOFF_MIN_MS, (unsigned)BACKOFF_MAX_MS);
    }
    if (draws[i] != draws[0]) {
      any_distinct = true;
    }
  }
  CHECK(all_in_range, "every backoff must lie inside the configured range");
  /* Section 9.7's entire purpose: several nodes detecting the same gunshot must
     not keep colliding at the same offset. A FIXED delay - min, max, or the
     midpoint - satisfies the range check above and defeats the mechanism. */
  CHECK(any_distinct,
        "all %zu backoffs were identical (%u ms): a fixed delay passes a bounds "
        "check while leaving colliding nodes in lockstep, which is the failure "
        "section 9.7 exists to prevent",
        count, (unsigned)draws[0]);
  scenario_end(&air);
}

static void test_tx_jitter_spreads_out_a_simultaneous_detection(void) {
  fake_air_t air;
  fake_air_init(&air);

  /* Section 9.7's opening requirement, and the one the section's own title is
     about: "gunshots or other common acoustic events may be detected by several
     nodes at almost the same time. If every node transmits immediately, collision
     probability is high." Backoff cannot address that - it only separates nodes
     that have ALREADY collided, at the cost of an ACK timeout each.
     This scenario is the situation itself: three nodes, one event, all three
     queueing in the same millisecond. */
  const uint32_t JITTER_MAX_MS = 40u;
  node_t         nodes[3];
  const uint32_t ids[3] = {NODE_A, NODE_B, NODE_C};
  for (size_t i = 0; i < 3u; i++) {
    REQUIRE(node_up_jittered(&nodes[i], &air, (uint8_t)i, ids[i], 3u, 0u, JITTER_MAX_MS),
            "node %zu came up", i);
  }

  const uint8_t detection[] = {0xD7};
  for (size_t i = 0; i < 3u; i++) {
    REQUIRE(boomlink_link_send(&nodes[i].link, 0x00000099u, BOOMLINK_TXPRIO_NORMAL, false,
                               detection, sizeof(detection)) == BOOMLINK_LINK_SEND_OK,
            "node %zu queued its detection", i);
    boomlink_link_poll(&nodes[i].link);
  }

  /* The property that matters: not one of them transmitted on that poll. Without
     jitter all three would be on the air already, in the same millisecond. */
  CHECK(fake_air_count(&air) == 0u,
        "no node may transmit immediately after a simultaneous detection, %zu on air",
        fake_air_count(&air));
  for (size_t i = 0; i < 3u; i++) {
    CHECK(boomlink_link_tx_state(&nodes[i].link) == BOOMLINK_TX_STATE_JITTER,
          "node %zu is waiting out its jitter, not backing off or idle", i);
    CHECK(boomlink_link_queue_depth(&nodes[i].link) == 0u,
          "node %zu's frame has left the queue - the sequence is assigned at "
          "dequeue (section 9.1), before the delay, not after",
          i);
  }

  /* Each node's own delay, measured. */
  uint32_t drawn[3];
  for (size_t i = 0; i < 3u; i++) {
    drawn[i] = ms_until_state_leaves(&nodes[i], BOOMLINK_TX_STATE_JITTER);
    CHECK(drawn[i] > 0u && drawn[i] <= JITTER_MAX_MS,
          "node %zu waited %u ms, outside (0, %u]", i, (unsigned)drawn[i],
          (unsigned)JITTER_MAX_MS);
  }
  CHECK(fake_air_count(&air) == 3u, "and then all three transmitted, %zu on air",
        fake_air_count(&air));

  /* The point of the whole mechanism: they did NOT all pick the same moment.
     A fixed delay - or a jitter drawn from a seed shared across nodes - passes
     every bounds check above and leaves the three in lockstep, which is the
     failure section 9.7 exists to prevent, just moved later in time. The fake
     seeds each node differently for exactly this reason (see fake_port_init). */
  CHECK(drawn[0] != drawn[1] || drawn[1] != drawn[2],
        "all three nodes drew the same delay (%u, %u, %u ms): they are still in "
        "lockstep and will still collide",
        (unsigned)drawn[0], (unsigned)drawn[1], (unsigned)drawn[2]);

  /* And a retransmission uses the BACKOFF, not the jitter - the two delays are
     distinct states so this is observable rather than inferred. */
  node_t acked;
  REQUIRE(node_up_jittered(&acked, &air, 3u, 0x00000044u, 3u, 0u, JITTER_MAX_MS),
          "a fourth node came up");
  REQUIRE(boomlink_link_send(&acked.link, 0x00000099u, BOOMLINK_TXPRIO_NORMAL, true,
                             detection, sizeof(detection)) == BOOMLINK_LINK_SEND_OK,
          "queued with an ACK requested");
  boomlink_link_poll(&acked.link);
  REQUIRE(boomlink_link_tx_state(&acked.link) == BOOMLINK_TX_STATE_JITTER,
          "the first attempt is jittered");
  REQUIRE(ms_until_state_leaves(&acked, BOOMLINK_TX_STATE_JITTER) > 0u,
          "the jitter expired and it transmitted");
  REQUIRE(boomlink_link_tx_state(&acked.link) == BOOMLINK_TX_STATE_WAIT_ACK,
          "and it now waits for the ACK");
  REQUIRE(ms_until_state_leaves(&acked, BOOMLINK_TX_STATE_WAIT_ACK) > 0u,
          "the ACK window expired");
  CHECK(boomlink_link_tx_state(&acked.link) == BOOMLINK_TX_STATE_BACKOFF,
        "a retry waits in BACKOFF, not in JITTER - jitter is for the first "
        "transmission only, and a retry that re-jittered would be applying the "
        "wrong distribution to a node that has already collided");
  const uint32_t backoff = ms_until_state_leaves(&acked, BOOMLINK_TX_STATE_BACKOFF);
  CHECK(backoff >= BACKOFF_MIN_MS && backoff <= BACKOFF_MAX_MS,
        "and it is drawn from the backoff range: %u ms, expected [%u, %u]",
        (unsigned)backoff, (unsigned)BACKOFF_MIN_MS, (unsigned)BACKOFF_MAX_MS);
  scenario_end(&air);
}

static void test_jitter_off_transmits_immediately(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a;
  /* tx_jitter_max_ms = 0 must mean no delay at all, not a zero-length one that
     still costs a poll. Every other scenario in this file depends on it - they
     queue, poll once, and assert on what is on the air - so if 0 stopped meaning
     "immediately" the failures would appear everywhere except here, where the
     behaviour is actually specified. */
  REQUIRE(node_up_jittered(&a, &air, 0u, NODE_A, 3u, 0u, 0u), "A came up unjittered");
  const uint8_t payload[] = {0x01};
  REQUIRE(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, false, payload,
                             sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
          "queued");
  boomlink_link_poll(&a.link);
  CHECK(fake_air_count(&air) == 1u,
        "with jitter disabled the frame goes out on the first poll, %zu on air",
        fake_air_count(&air));
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_IDLE,
        "and the pipeline is free again - it never entered a delay state");
  scenario_end(&air);
}

static void test_the_ack_window_is_derived_from_the_frame_not_fixed(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t small, big;
  REQUIRE(node_up_full(&small, &air, 0u, NODE_A, 1u, 0u), "the short-frame node came up");
  REQUIRE(node_up_full(&big, &air, 1u, NODE_B, 1u, 0u), "the long-frame node came up");

  static uint8_t long_payload[200];
  memset(long_payload, 0x77, sizeof(long_payload));
  const uint8_t short_payload[] = {0x01};

  REQUIRE(boomlink_link_send(&small.link, NODE_C, BOOMLINK_TXPRIO_NORMAL, true,
                             short_payload,
                             sizeof(short_payload)) == BOOMLINK_LINK_SEND_OK,
          "the short frame is queued");
  REQUIRE(boomlink_link_send(&big.link, NODE_C, BOOMLINK_TXPRIO_NORMAL, true, long_payload,
                             sizeof(long_payload)) == BOOMLINK_LINK_SEND_OK,
          "the long frame is queued");
  boomlink_link_poll(&small.link);
  boomlink_link_poll(&big.link);

  const uint32_t short_window = ms_until_state_leaves(&small, BOOMLINK_TX_STATE_WAIT_ACK);
  const uint32_t long_window  = ms_until_state_leaves(&big, BOOMLINK_TX_STATE_WAIT_ACK);
  CHECK(short_window > MARGIN_MS, "the short frame's window (%u ms) exceeds the margin",
        (unsigned)short_window);
  /* Section 9.6: the timeout must be "derived from/configured for the active radio
     profile rather than assuming one fixed timeout for every spreading factor and
     packet size". A constant window passes every other timing assertion in this
     file; only comparing two frame lengths distinguishes it. At SF12 a full packet
     is seconds of airtime, so a fixed window sized for a short frame would expire
     before a long one had finished going out - and every long frame would be
     retried three times and reported as failed. */
  CHECK(long_window > short_window,
        "a %zu-byte payload must get a longer ACK window than a %zu-byte one: %u ms "
        "vs %u ms",
        sizeof(long_payload), sizeof(short_payload), (unsigned)long_window,
        (unsigned)short_window);
  scenario_end(&air);
}

static void test_a_broadcast_is_sent_once_and_never_awaits_an_ack(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");

  /* request_ack = true on a broadcast, which section 9.9 forbids and the engine
     forces off. If the ACK request survived, this frame would enter the ACK wait,
     nobody would answer it (an ACK to the broadcast address is not a valid ACK),
     and it would be retried three times - burning three times the airtime of the
     N:1 storm section 9.5 is worried about, from the sender's own side. */
  const uint8_t payload[] = {0xEE};
  REQUIRE(boomlink_link_send(&a.link, BOOMLINK_ADDR_BROADCAST, BOOMLINK_TXPRIO_NORMAL,
                             true, payload, sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
          "queued");
  boomlink_link_poll(&a.link);
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_IDLE,
        "a broadcast never enters the ACK wait");
  REQUIRE(a.tx.calls == 1u, "and is finished immediately, got %u call(s)", a.tx.calls);
  CHECK(a.tx.last_outcome == BOOMLINK_TX_SENT,
        "reported as sent - not as acknowledged, which nothing established");
  CHECK(a.tx.last_destination == BOOMLINK_ADDR_BROADCAST, "to the broadcast address");
  /* Nothing was received for a SENT outcome, so there is no reading to report -
     0.0f is this codebase's existing sentinel for that, not a fabricated
     measurement. */
  CHECK(a.tx.last_rssi_dbm == 0.0f, "no signal quality is reported for SENT, got %f",
        (double)a.tx.last_rssi_dbm);
  CHECK(a.tx.last_snr_db == 0.0f, "same for SNR");

  fake_port_advance_ms(&a.ctx, 100000u);
  boomlink_link_poll(&a.link);
  CHECK(fake_air_count(&air) == 1u, "and never retried, %zu on air",
        fake_air_count(&air));
  boomlink_link_stats_t as;
  boomlink_link_get_stats(&a.link, &as);
  CHECK(as.tx_retries == 0u, "no retries");
  CHECK(as.tx_failures == 0u, "and no failure - an unacknowledged broadcast is normal");
  scenario_end(&air);
}

static void test_reconfigure_applies_live_without_disturbing_a_pending_frame(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a;
  /* max_attempts=1, so the frame currently in flight would give up on its very
     first timeout under the ORIGINAL policy - which is exactly what makes a
     change taking effect on it observable, rather than only on the next frame. */
  REQUIRE(node_up_full(&a, &air, 0u, NODE_A, 1u, 0u), "A came up with one attempt");

  const uint8_t payload[] = {0x5Eu};
  REQUIRE(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, true, payload,
                             sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
          "queued");
  boomlink_link_poll(&a.link);
  REQUIRE(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_WAIT_ACK,
          "the first attempt is on the air and waiting");
  boomlink_linkframe_header_t first;
  size_t                      ignored = 0u;
  REQUIRE(parse_air(&air, 0u, &first, &ignored), "it parses");

  /* Reconfigure WHILE that frame is pending. section 8.2's LinkConfig is a
     runtime-updatable message group - this is what makes the update apply to a
     live link mean something, rather than only to whatever gets queued next. */
  const uint32_t NEW_BACKOFF_MS = 5u;
  REQUIRE(boomlink_link_reconfigure(&a.link, MARGIN_MS, 3u, NEW_BACKOFF_MS, NEW_BACKOFF_MS,
                                    0u),
          "a valid policy is accepted");
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_WAIT_ACK,
        "the pending frame's state is untouched by reconfiguring");

  /* Under the OLD policy (max_attempts=1) this timeout would have been final
     failure. Under the new one (max_attempts=3) it must retry instead - proof
     the live pipeline is reading the UPDATED policy, not a snapshot taken when
     the frame was dequeued. */
  const uint32_t window = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_WAIT_ACK);
  CHECK(window > MARGIN_MS, "the window was measured");
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_BACKOFF,
        "reconfigured max_attempts (3) permits a retry the old policy (1) would not have");
  const uint32_t backoff = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_BACKOFF);
  CHECK(backoff == NEW_BACKOFF_MS,
        "and the backoff comes from the NEW range (fixed at %u ms), got %u - the old "
        "[%u, %u] range must not still be in effect",
        (unsigned)NEW_BACKOFF_MS, (unsigned)backoff, (unsigned)BACKOFF_MIN_MS,
        (unsigned)BACKOFF_MAX_MS);

  REQUIRE(fake_air_count(&air) == 2u, "the retry went out, %zu on air", fake_air_count(&air));
  boomlink_linkframe_header_t retry;
  REQUIRE(parse_air(&air, 1u, &retry, &ignored), "it parses");
  CHECK(retry.sequence == first.sequence && retry.session_id == first.session_id,
        "it is the SAME frame retried, not a fresh one from the queue - "
        "reconfigure must not touch the session, sequence, or the pending item");

  /* One more round trip through the new policy, ending in final failure at
     attempt 3 as the new max_attempts promises. */
  const uint32_t window2 = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_WAIT_ACK);
  CHECK(window2 > MARGIN_MS, "second window measured");
  const uint32_t backoff2 = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_BACKOFF);
  CHECK(backoff2 == NEW_BACKOFF_MS, "second backoff still from the new range, got %u",
        (unsigned)backoff2);
  const uint32_t window3 = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_WAIT_ACK);
  CHECK(window3 > MARGIN_MS, "third window measured");
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_IDLE,
        "after exactly 3 attempts the reconfigured budget is exhausted");
  REQUIRE(a.tx.calls == 1u, "one outcome reported, got %u", a.tx.calls);
  CHECK(a.tx.last_outcome == BOOMLINK_TX_NO_ACK, "as a failure");
  CHECK(a.tx.last_attempts == 3u, "after 3 attempts - the reconfigured budget, got %u",
        (unsigned)a.tx.last_attempts);

  /* Rejections leave the policy exactly as the last SUCCESSFUL call left it. */
  CHECK(!boomlink_link_reconfigure(&a.link, MARGIN_MS, 0u, NEW_BACKOFF_MS, NEW_BACKOFF_MS, 0u),
        "zero attempts is refused");
  CHECK(!boomlink_link_reconfigure(&a.link, MARGIN_MS, 3u, 100u, 10u, 0u),
        "an inverted backoff range is refused");
  CHECK(!boomlink_link_reconfigure(NULL, MARGIN_MS, 3u, NEW_BACKOFF_MS, NEW_BACKOFF_MS, 0u),
        "a NULL link is refused rather than dereferenced");

  /* Prove the rejections truly left nothing changed: send one more frame and
     confirm it still runs the 3-attempt / 5 ms policy from the successful call
     above, not a partially-applied mix of it and a rejected one. */
  REQUIRE(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, true, payload,
                             sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
          "a second frame queued");
  boomlink_link_poll(&a.link);
  const uint32_t window4 = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_WAIT_ACK);
  CHECK(window4 > MARGIN_MS, "window measured");
  const uint32_t backoff4 = ms_until_state_leaves(&a, BOOMLINK_TX_STATE_BACKOFF);
  CHECK(backoff4 == NEW_BACKOFF_MS,
        "still the policy from the last SUCCESSFUL reconfigure, got %u ms",
        (unsigned)backoff4);
  scenario_end(&air);
}

static void test_send_refuses_what_can_never_be_transmitted(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");

  const uint8_t payload[] = {0x01};
  /* Section 9.1's "enforce destination rules". Rejected rather than silently
     corrected: there is no sensible substitute for "nobody" or for "myself". */
  CHECK(boomlink_link_send(&a.link, BOOMLINK_ADDR_INVALID, BOOMLINK_TXPRIO_NORMAL, false,
                           payload, sizeof(payload)) == BOOMLINK_LINK_SEND_BAD_DESTINATION,
        "the unconfigured address is not a destination");
  CHECK(boomlink_link_send(&a.link, NODE_A, BOOMLINK_TXPRIO_NORMAL, false, payload,
                           sizeof(payload)) == BOOMLINK_LINK_SEND_BAD_DESTINATION,
        "and neither is this node itself - the receiving half of this very engine "
        "would reject the frame as an impossible source, after paying its airtime");
  CHECK(boomlink_link_send(&a.link, BOOMLINK_ADDR_BROADCAST, BOOMLINK_TXPRIO_NORMAL,
                           false, payload, sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
        "broadcast, however, is a perfectly good destination");

  boomlink_link_stats_t as;
  boomlink_link_get_stats(&a.link, &as);
  CHECK(as.tx_dropped == 2u, "both refusals are visible in the statistics, got %u",
        (unsigned)as.tx_dropped);
  CHECK(fake_air_count(&air) == 0u, "and nothing was transmitted by send()");

  /* The payload ceiling, against the PORT. On a reduced radio profile this is
     tighter than the queue's compile-time slot size, and that gap is the bug: a
     payload that fits a slot but not the radio used to be accepted, reported as
     queued, and then destroyed at transmission time. */
  node_t small;
  REQUIRE(node_up_full(&small, &air, 1u, NODE_B, 3u, 64u), "a 64-byte profile came up");
  static uint8_t big[BOOMLINK_TX_MAX_PAYLOAD];
  memset(big, 0x5Au, sizeof(big));
  const size_t fits = 64u - BOOMLINK_LINKFRAME_HEADER_SIZE;
  CHECK(boomlink_link_send(&small.link, NODE_C, BOOMLINK_TXPRIO_NORMAL, false, big,
                           fits + 1u) == BOOMLINK_LINK_SEND_TOO_LONG,
        "one byte past what a 64-byte radio can carry must be refused at send()");
  CHECK(boomlink_link_send(&small.link, NODE_C, BOOMLINK_TXPRIO_NORMAL, false, big,
                           fits) == BOOMLINK_LINK_SEND_OK,
        "and exactly what fits must be accepted, or the largest legal frame can "
        "never be sent");
  CHECK(boomlink_link_send(&small.link, NODE_C, BOOMLINK_TXPRIO_NORMAL, false, big,
                           sizeof(big)) == BOOMLINK_LINK_SEND_TOO_LONG,
        "a payload that fits a queue SLOT but not this radio is refused too - which "
        "the queue's own compile-time bound cannot see");
  boomlink_link_get_stats(&small.link, &as);
  CHECK(as.tx_dropped == 2u, "both oversize refusals counted, got %u",
        (unsigned)as.tx_dropped);

  /* And the accepted one really does go out, so the boundary is not off by one in
     the safe-looking direction. */
  boomlink_link_poll(&small.link);
  REQUIRE(fake_air_count(&air) == 1u, "the largest legal frame was transmitted");
  const fake_transmission_t *tx = fake_air_transmission(&air, 0u);
  REQUIRE(tx != NULL, "logged");
  CHECK(tx->len == 64u, "at exactly the radio's limit, got %zu bytes", tx->len);
  scenario_end(&air);
}

static void test_queue_pressure_is_visible_in_the_statistics(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");

  /* Fill the queue with telemetry. Nothing is polled, so nothing leaves. */
  const uint8_t payload[] = {0x01};
  for (size_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS; i++) {
    REQUIRE(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_LOW, false, payload,
                               sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
            "telemetry %zu queued", i);
  }
  boomlink_link_stats_t as;
  boomlink_link_get_stats(&a.link, &as);
  CHECK(as.tx_dropped == 0u, "filling the queue drops nothing");
  CHECK(as.tx_shed == 0u, "and sheds nothing");

  /* More telemetry into a queue full of telemetry: refused, not displacing a
     reading that has waited longer. Without a counter here, a node generating
     more traffic than the link can carry would look identical to a quiet one -
     every statistic section 9.10 lists describes what happened after queueing. */
  CHECK(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_LOW, false, payload,
                           sizeof(payload)) == BOOMLINK_LINK_SEND_QUEUE_FULL,
        "a full queue refuses equally unurgent traffic");
  boomlink_link_get_stats(&a.link, &as);
  CHECK(as.tx_dropped == 1u, "and that is counted, got %u", (unsigned)as.tx_dropped);

  /* A detection event displaces telemetry - section 9.8's policy working, which is
     why it is counted apart from a drop. */
  CHECK(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, false, payload,
                           sizeof(payload)) == BOOMLINK_LINK_SEND_OK_EVICTED,
        "a detection event sheds telemetry to get in");
  boomlink_link_get_stats(&a.link, &as);
  CHECK(as.tx_shed == 1u, "counted as shed, got %u", (unsigned)as.tx_shed);
  CHECK(as.tx_dropped == 1u,
        "and NOT as a drop: a node protecting detections is the policy working, not "
        "a node in trouble");
  CHECK(boomlink_link_queue_depth(&a.link) == BOOMLINK_TXQUEUE_SLOTS,
        "the queue is still exactly full, got %zu", boomlink_link_queue_depth(&a.link));
  scenario_end(&air);
}

static void test_two_engines_exchange_traffic_in_both_directions(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a, b;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  /* Both directions at once, each asking for an ACK, which is the arrangement
     where a shared-state mistake shows: one node's pending frame satisfied by the
     other's ACK, or a duplicate cache keyed loosely enough that A's sequence 1
     suppresses B's sequence 1. Both engines run the same code on one medium, so
     nothing here is simulated. */
  const uint8_t a_says[] = {0xAA, 0xAB};
  const uint8_t b_says[] = {0xBB};
  REQUIRE(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, true, a_says,
                             sizeof(a_says)) == BOOMLINK_LINK_SEND_OK,
          "A queued its frame");
  REQUIRE(boomlink_link_send(&b.link, NODE_A, BOOMLINK_TXPRIO_NORMAL, true, b_says,
                             sizeof(b_says)) == BOOMLINK_LINK_SEND_OK,
          "B queued its frame");

  /* Interleaved polling, the way a superloop on each board would run. */
  for (int round = 0; round < 4; round++) {
    boomlink_link_poll(&a.link);
    boomlink_link_poll(&b.link);
  }

  CHECK(b.rx.calls == 1u, "B received A's frame exactly once, got %u", b.rx.calls);
  CHECK(b.rx.last_source == NODE_A, "from A");
  CHECK(b.rx.last_len == sizeof(a_says), "with its payload");
  CHECK(a.rx.calls == 1u, "A received B's frame exactly once, got %u", a.rx.calls);
  CHECK(a.rx.last_source == NODE_B, "from B");
  CHECK(a.rx.last_len == sizeof(b_says), "with its payload");

  boomlink_link_stats_t as, bs;
  boomlink_link_get_stats(&a.link, &as);
  boomlink_link_get_stats(&b.link, &bs);
  CHECK(as.ack_received == 1u, "A's frame was acknowledged, got %u",
        (unsigned)as.ack_received);
  CHECK(bs.ack_received == 1u, "and B's was too, got %u", (unsigned)bs.ack_received);
  CHECK(as.ack_unmatched == 0u, "A matched no ACK it should not have, got %u",
        (unsigned)as.ack_unmatched);
  CHECK(bs.ack_unmatched == 0u, "nor did B, got %u", (unsigned)bs.ack_unmatched);
  CHECK(as.tx_retries == 0u && bs.tx_retries == 0u, "and neither needed a retry");
  CHECK(a.tx.acked == 1u && b.tx.acked == 1u, "both reported delivery");
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_IDLE, "A's pipeline is free");
  CHECK(boomlink_link_tx_state(&b.link) == BOOMLINK_TX_STATE_IDLE, "and B's");
  CHECK(fake_air_count(&air) == 4u, "two frames and two ACKs, got %zu",
        fake_air_count(&air));
  scenario_end(&air);
}

int main(void) {
  test_an_ack_completes_the_pending_frame();
  test_an_unanswered_frame_is_retried_with_the_same_sequence();
  test_the_queue_is_held_while_a_frame_awaits_its_ack();
  test_a_refused_send_keeps_the_frame_and_its_order();
  test_a_near_miss_ack_leaves_the_frame_pending();
  test_the_backoff_is_randomized_within_its_bounds();
  test_tx_jitter_spreads_out_a_simultaneous_detection();
  test_jitter_off_transmits_immediately();
  test_the_ack_window_is_derived_from_the_frame_not_fixed();
  test_a_broadcast_is_sent_once_and_never_awaits_an_ack();
  test_reconfigure_applies_live_without_disturbing_a_pending_frame();
  test_send_refuses_what_can_never_be_transmitted();
  test_queue_pressure_is_visible_in_the_statistics();
  test_two_engines_exchange_traffic_in_both_directions();
  BOOMLINK_TEST_REPORT("link_tx_test", 271);
}
