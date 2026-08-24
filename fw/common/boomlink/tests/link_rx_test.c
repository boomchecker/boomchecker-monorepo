/**
 ******************************************************************************
 * @file    link_rx_test.c
 * @brief   The link engine's RX pipeline (boomlink.md section 9.2), and the
 *          end-to-end delivery that pipeline exists for.
 *
 *          Every scenario runs TWO engines on one fake medium, so a frame is
 *          built by one and validated by the other. That is not ceremony: a test
 *          that constructed the expected frame itself and fed it in would agree
 *          with a misreading of section 7.3 as happily as the code does.
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

/* What the on_rx callback saw, so a scenario can assert on delivery rather than
   only on counters - the difference between what arrived and what the receiver
   believes arrived. */
typedef struct {
  unsigned calls;
  uint32_t last_source;
  uint8_t  last_payload[64];
  size_t   last_len;
} rx_log_t;

static void rx_capture(void *user, uint32_t source_id, const uint8_t *payload,
                       size_t payload_len) {
  rx_log_t *log     = (rx_log_t *)user;
  log->calls++;
  log->last_source  = source_id;
  log->last_len     = payload_len;
  if (payload_len > sizeof(log->last_payload)) {
    payload_len = sizeof(log->last_payload);
  }
  memcpy(log->last_payload, payload, payload_len);
}

/** A node: engine, port, fake context and its delivery log. */
typedef struct {
  boomlink_link_t link;
  boomlink_port_t port;
  fake_port_ctx_t ctx;
  rx_log_t        rx;
} node_t;

static bool node_up(node_t *n, fake_air_t *air, uint8_t index, uint32_t node_id) {
  memset(n, 0, sizeof(*n));
  fake_port_init(&n->ctx, air, index, 0x1000u + node_id, &n->port);
  boomlink_link_config_t cfg = {
      .node_id               = node_id,
      .magic                 = BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
      .ack_timeout_margin_ms = 50u,
      .max_attempts          = 3u,
      .backoff_min_ms        = 20u,
      .backoff_max_ms        = 60u,
      .on_rx                 = rx_capture,
      .on_rx_user            = &n->rx,
  };
  /* Session per boot (section 9.3), distinct per node so a scenario cannot pass
     by confusing two peers' state. */
  return boomlink_link_init(&n->link, &cfg, &n->port, 0x5000u + node_id);
}

static void test_init_refuses_an_unusable_configuration(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t          n;
  fake_port_ctx_t ctx;
  boomlink_port_t port;
  fake_port_init(&ctx, &air, 0u, 1u, &port);

  boomlink_link_config_t base = {
      .node_id               = NODE_A,
      .magic                 = BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
      .ack_timeout_margin_ms = 50u,
      .max_attempts          = 3u,
      .backoff_min_ms        = 10u,
      .backoff_max_ms        = 20u,
  };
  CHECK(boomlink_link_init(&n.link, &base, &port, 1u), "a sane config is accepted");

  /* Each of these is a misconfiguration that would otherwise look like working
     hardware with a silent link, which is the worst kind to debug in the field. */
  boomlink_link_config_t bad = base;
  bad.node_id                = BOOMLINK_ADDR_INVALID;
  CHECK(!boomlink_link_init(&n.link, &bad, &port, 1u),
        "an unconfigured node_id must be refused: such a node accepts nothing "
        "(section 7.2) and can acknowledge nothing");
  bad         = base;
  bad.node_id = BOOMLINK_ADDR_BROADCAST;
  CHECK(!boomlink_link_init(&n.link, &bad, &port, 1u),
        "the broadcast address is not something a node can BE");
  bad              = base;
  bad.max_attempts = 0u;
  CHECK(!boomlink_link_init(&n.link, &bad, &port, 1u),
        "zero attempts means queued frames are never sent at all");
  bad                = base;
  bad.backoff_min_ms = 100u;
  bad.backoff_max_ms = 50u;
  CHECK(!boomlink_link_init(&n.link, &bad, &port, 1u), "an inverted backoff range");

  boomlink_port_t broken = port;
  broken.send            = NULL;
  CHECK(!boomlink_link_init(&n.link, &base, &broken, 1u), "an invalid port");
}

static void test_a_unicast_frame_reaches_its_destination_and_nobody_else(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a, b, c;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");
  REQUIRE(node_up(&c, &air, 2u, NODE_C), "C came up");

  const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
  CHECK(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, false, payload,
                           sizeof(payload)) == BOOMLINK_TXQUEUE_OK,
        "queued");
  /* Nothing on the air until the link is serviced: section 9.1 assigns the
     sequence at dequeue, so send() cannot have transmitted. */
  CHECK(fake_air_count(&air) == 0u, "send() must not transmit");
  boomlink_link_poll(&a.link);
  CHECK(fake_air_count(&air) == 1u, "poll transmitted it");

  boomlink_link_poll(&b.link);
  boomlink_link_poll(&c.link);

  CHECK(b.rx.calls == 1u, "B received it");
  CHECK(b.rx.last_source == NODE_A, "from A, got %08X", (unsigned)b.rx.last_source);
  CHECK(b.rx.last_len == sizeof(payload), "with the payload intact");
  CHECK(memcmp(b.rx.last_payload, payload, sizeof(payload)) == 0, "byte for byte");
  /* Section 7.2's whole point. C hears the packet - it is a radio - and must
     drop it, counting it as someone else's rather than as malformed. */
  CHECK(c.rx.calls == 0u, "C must not receive a frame addressed to B");
  boomlink_link_stats_t cs;
  boomlink_link_get_stats(&c.link, &cs);
  CHECK(cs.rx_other_destination == 1u, "C counted it as another destination, got %u",
        (unsigned)cs.rx_other_destination);
  CHECK(cs.rx_malformed == 0u, "and not as malformed - it was a perfectly good frame");
}

static void test_broadcast_reaches_everyone_and_asks_for_no_ack(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a, b, c;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");
  REQUIRE(node_up(&c, &air, 2u, NODE_C), "C came up");

  const uint8_t payload[] = {0x01, 0x02};
  /* request_ack = true on a broadcast. Section 9.9 forbids it, and the engine
     forces it off rather than refusing the call - the caller should not have to
     know the rule. If it did NOT, every node in range would answer and section
     9.5's ACK storm is exactly what happens. */
  CHECK(boomlink_link_send(&a.link, BOOMLINK_ADDR_BROADCAST, BOOMLINK_TXPRIO_NORMAL, true,
                           payload, sizeof(payload)) == BOOMLINK_TXQUEUE_OK,
        "queued");
  boomlink_link_poll(&a.link);
  boomlink_link_poll(&b.link);
  boomlink_link_poll(&c.link);

  CHECK(b.rx.calls == 1u, "B received the broadcast");
  CHECK(c.rx.calls == 1u, "C received the broadcast");
  CHECK(fake_air_count(&air) == 1u,
        "exactly one transmission: nobody acknowledged a broadcast (%zu on air)",
        fake_air_count(&air));

  /* The EMITTED BYTES, not just the outcome. Two independent mechanisms keep a
     broadcast from being acknowledged - this sender-side rule (section 9.9,
     "broadcast never requests link ACK") and the receiver-side refusal to
     acknowledge a broadcast-addressed frame (section 9.5). They are deliberate
     defence in depth, but the counters above observe only their combined effect,
     so either one could be deleted with the suite green. Verified: removing the
     sender-side force-off left every assertion above passing, because the
     receivers declined anyway.
     The receiver-side half is pinned independently by the forged-frame scenario
     below, which bypasses this sender entirely. */
  const fake_transmission_t *tx = fake_air_transmission(&air, 0u);
  REQUIRE(tx != NULL, "the broadcast was logged");
  boomlink_linkframe_header_t sent;
  size_t                      sent_payload = 0u;
  REQUIRE(boomlink_linkframe_parse(tx->bytes, tx->len, BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
                                   &sent, &sent_payload) == BOOMLINK_LINKFRAME_OK,
          "the broadcast parses");
  CHECK(sent.destination_id == BOOMLINK_ADDR_BROADCAST, "it really is a broadcast");
  CHECK(!sent.ack_requested,
        "the ACK request must be stripped before transmission, whatever the caller "
        "asked for: flags byte on air was %#04x", tx->bytes[2]);

  boomlink_link_stats_t bs, cs;
  boomlink_link_get_stats(&b.link, &bs);
  boomlink_link_get_stats(&c.link, &cs);
  CHECK(bs.ack_sent == 0u, "B sent no ACK");
  CHECK(cs.ack_sent == 0u, "C sent no ACK");
}

static void test_a_requested_ack_is_sent_and_is_the_frame_layers_ack(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a, b;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  const uint8_t payload[] = {0x42};
  CHECK(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, true, payload,
                           sizeof(payload)) == BOOMLINK_TXQUEUE_OK,
        "queued");
  boomlink_link_poll(&a.link);
  REQUIRE(fake_air_count(&air) == 1u, "the DATA frame is on the air");

  /* Keep the DATA frame's header so the ACK can be checked against it field by
     field, rather than against a hand-written expectation that could share a
     misreading with the code. */
  const fake_transmission_t *data_tx = fake_air_transmission(&air, 0u);
  REQUIRE(data_tx != NULL, "the transmission was logged");
  boomlink_linkframe_header_t sent;
  size_t                      sent_payload = 0u;
  REQUIRE(boomlink_linkframe_parse(data_tx->bytes, data_tx->len,
                                   BOOMLINK_LINKFRAME_MAGIC_DEFAULT, &sent,
                                   &sent_payload) == BOOMLINK_LINKFRAME_OK,
          "the DATA frame parses");
  CHECK(sent.ack_requested, "and it asked for an ACK");
  CHECK(sent.sequence == 1u, "the first sequence of the session is 1, got %u",
        (unsigned)sent.sequence);

  boomlink_link_poll(&b.link);
  REQUIRE(fake_air_count(&air) == 2u, "B answered with an ACK (%zu on air)",
          fake_air_count(&air));

  const fake_transmission_t *ack_tx = fake_air_transmission(&air, 1u);
  REQUIRE(ack_tx != NULL, "the ACK was logged");
  CHECK(ack_tx->len == BOOMLINK_LINKFRAME_HEADER_SIZE,
        "an ACK carries no payload (section 9.5), got %zu bytes", ack_tx->len);
  boomlink_linkframe_header_t ack;
  size_t                      ack_payload = 0u;
  REQUIRE(boomlink_linkframe_parse(ack_tx->bytes, ack_tx->len,
                                   BOOMLINK_LINKFRAME_MAGIC_DEFAULT, &ack,
                                   &ack_payload) == BOOMLINK_LINKFRAME_OK,
          "the ACK parses");

  /* The ACK the engine emitted must be exactly the one the frame layer's pinned
     mapping produces from the frame that was actually sent. This is what ties
     the engine to section 9.5 without restating the mapping here. */
  boomlink_linkframe_header_t expected;
  REQUIRE(boomlink_linkframe_make_ack(&sent, NODE_B, &expected),
          "the frame layer can build the ACK");
  uint8_t expected_bytes[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&expected, expected_bytes);
  CHECK(memcmp(ack_tx->bytes, expected_bytes, sizeof(expected_bytes)) == 0,
        "the engine's ACK differs from boomlink_linkframe_make_ack()'s");
  /* And it is one this node can match, which is what the sender will do. */
  CHECK(boomlink_linkframe_ack_matches(&sent, &ack, NODE_A),
        "the sender must be able to match its own frame's ACK");

  boomlink_link_stats_t bs;
  boomlink_link_get_stats(&b.link, &bs);
  CHECK(bs.ack_sent == 1u, "B counted the ACK");
  CHECK(bs.rx_envelopes == 1u, "and delivered the payload");
}

static void test_a_duplicate_is_not_delivered_twice_but_is_acknowledged_again(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a, b;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  const uint8_t payload[] = {0x77};
  CHECK(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, true, payload,
                           sizeof(payload)) == BOOMLINK_TXQUEUE_OK,
        "queued");
  boomlink_link_poll(&a.link);
  REQUIRE(fake_air_count(&air) == 1u, "on the air");

  const fake_transmission_t *tx = fake_air_transmission(&air, 0u);
  REQUIRE(tx != NULL, "logged");
  uint8_t copy[FAKE_PORT_MAX_PACKET];
  const size_t copy_len = tx->len;
  memcpy(copy, tx->bytes, copy_len);

  boomlink_link_poll(&b.link);
  CHECK(b.rx.calls == 1u, "delivered once");

  /* Replay the identical frame, which is exactly what a retry looks like
     (section 9.6 reuses the same session and sequence on purpose). */
  fake_air_inject(&air, 0u, copy, copy_len);
  boomlink_link_poll(&b.link);

  CHECK(b.rx.calls == 1u,
        "section 9.4: application handlers must see an ACKed message at most once "
        "- delivered %u times", b.rx.calls);
  boomlink_link_stats_t bs;
  boomlink_link_get_stats(&b.link, &bs);
  CHECK(bs.rx_duplicates == 1u, "the duplicate was counted, got %u",
        (unsigned)bs.rx_duplicates);
  CHECK(bs.rx_envelopes == 1u, "and only one envelope was accepted");
  /* Section 9.4: "if it is unicast and requests ACK, transmit the ACK again". The
     duplicate arrived BECAUSE the sender did not hear the first ACK, so silence
     here would guarantee it keeps retrying until it gives up. */
  CHECK(bs.ack_sent == 2u,
        "the ACK must be resent for a duplicate, sent %u", (unsigned)bs.ack_sent);
}

static void test_foreign_and_malformed_traffic_are_counted_apart(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t b;
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  /* A frame from another deployment sharing the channel. Section 7.3 requires it
     dropped before further processing, and section 9.2 counts it separately from
     malformed traffic - a neighbouring network is not corruption, and conflating
     them would make a perfectly healthy channel look broken. */
  boomlink_linkframe_header_t foreign;
  boomlink_linkframe_header_init(&foreign);
  foreign.magic          = 0x5Au;
  foreign.destination_id = NODE_B;
  foreign.source_id      = NODE_A;
  foreign.session_id     = 1u;
  foreign.sequence       = 1u;
  uint8_t bytes[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&foreign, bytes);
  fake_air_inject(&air, 0u, bytes, sizeof(bytes));

  /* Truncated: not a frame at all. */
  fake_air_inject(&air, 0u, bytes, BOOMLINK_LINKFRAME_HEADER_SIZE - 1u);

  /* Longer than the radio can carry, so only a prefix was copied and the rest is
     unknowable - counted apart again, because a flood of these means a foreign
     transmitter or a driver fault, not corrupt traffic. */
  uint8_t huge[FAKE_PORT_MAX_PACKET];
  memset(huge, 0xAB, sizeof(huge));
  fake_air_inject(&air, 0u, huge, sizeof(huge) + 1u);

  boomlink_link_poll(&b.link);

  boomlink_link_stats_t s;
  boomlink_link_get_stats(&b.link, &s);
  CHECK(s.rx_rejected_magic_or_version == 1u, "the foreign frame, got %u",
        (unsigned)s.rx_rejected_magic_or_version);
  CHECK(s.rx_malformed == 1u, "the truncated frame, got %u", (unsigned)s.rx_malformed);
  CHECK(s.rx_envelopes == 0u, "nothing was delivered");
  CHECK(b.rx.calls == 0u, "and the callback was never reached");
  CHECK(s.ack_sent == 0u, "none of it was acknowledged");
}

static void test_a_broadcast_asking_for_an_ack_is_never_acknowledged(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t b, c;
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");
  REQUIRE(node_up(&c, &air, 2u, NODE_C), "C came up");

  /* No compliant sender emits this (section 9.9), so it has to be forged - and
     that is the point. Section 9.5 records "do not acknowledge a frame addressed
     to the broadcast address" as a receiver-side responsibility the frame layer
     cannot take on, because make_ack() builds such an ACK quite happily. This is
     the real ACK-storm vector: one frame, answered by every node in range. */
  boomlink_linkframe_header_t hostile;
  boomlink_linkframe_header_init(&hostile);
  hostile.magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT;
  hostile.destination_id = BOOMLINK_ADDR_BROADCAST;
  hostile.source_id      = NODE_A;
  hostile.session_id     = 9u;
  hostile.sequence       = 1u;
  hostile.ack_requested  = true;
  uint8_t bytes[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&hostile, bytes);
  CHECK(bytes[2] == BOOMLINK_LINKFRAME_FLAG_ACK_REQUESTED,
        "the forged frame really does request an ACK");
  fake_air_inject(&air, 0u, bytes, sizeof(bytes));

  boomlink_link_poll(&b.link);
  boomlink_link_poll(&c.link);

  boomlink_link_stats_t bs, cs;
  boomlink_link_get_stats(&b.link, &bs);
  boomlink_link_get_stats(&c.link, &cs);
  /* Both must accept the payload - it is a broadcast - and neither may answer. */
  CHECK(bs.rx_envelopes == 1u, "B accepted the broadcast payload");
  CHECK(cs.rx_envelopes == 1u, "C accepted the broadcast payload");
  CHECK(bs.ack_sent == 0u, "B must not acknowledge a broadcast");
  CHECK(cs.ack_sent == 0u, "C must not acknowledge a broadcast");
  CHECK(fake_air_count(&air) == 1u,
        "no ACK reached the air: %zu transmissions, expected just the forged one",
        fake_air_count(&air));
}

static void test_an_unmatched_ack_is_counted_not_delivered(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");

  /* An ACK for something this node is not waiting for: late, or forged. It must
     not reach the application and must not be mistaken for a data frame. */
  boomlink_linkframe_header_t stray;
  boomlink_linkframe_header_init(&stray);
  stray.magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT;
  stray.frame_type     = BOOMLINK_FRAME_TYPE_ACK;
  stray.destination_id = NODE_A;
  stray.source_id      = NODE_B;
  stray.session_id     = 0x1234u;
  stray.sequence       = 0x5678u;
  uint8_t bytes[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&stray, bytes);
  fake_air_inject(&air, 1u, bytes, sizeof(bytes));

  boomlink_link_poll(&a.link);

  boomlink_link_stats_t s;
  boomlink_link_get_stats(&a.link, &s);
  CHECK(a.rx.calls == 0u, "an ACK is not application data");
  CHECK(s.rx_envelopes == 0u, "and is not counted as one");
  CHECK(s.ack_unmatched == 1u, "it was counted as unmatched, got %u",
        (unsigned)s.ack_unmatched);
  CHECK(s.rx_malformed == 0u, "a stray ACK is not malformed - it is a valid frame");
  CHECK(s.ack_sent == 0u, "and an ACK is never acknowledged");
}

static void test_signal_quality_and_airtime_are_recorded(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a, b;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  const uint8_t payload[] = {1u, 2u, 3u};
  (void)boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, false, payload,
                           sizeof(payload));
  boomlink_link_poll(&a.link);
  boomlink_link_poll(&b.link);

  boomlink_link_stats_t as, bs;
  boomlink_link_get_stats(&a.link, &as);
  boomlink_link_get_stats(&b.link, &bs);
  CHECK(as.tx_envelopes == 1u, "A counted the transmission");
  /* Section 9.10's cumulative TX airtime, for section 6.1's duty cycle. Must
     scale with the frame, or a duty-cycle budget computed from it is fiction. */
  CHECK(as.tx_airtime_us > 0u, "airtime was accumulated");
  CHECK(as.tx_airtime_us ==
            (uint64_t)a.port.airtime_us(a.port.ctx,
                                        BOOMLINK_LINKFRAME_HEADER_SIZE + sizeof(payload)),
        "airtime must match the frame's actual length");
  CHECK(bs.last_rssi_dbm < 0.0f, "B recorded RSSI, got %f", (double)bs.last_rssi_dbm);
  CHECK(bs.last_snr_db > 0.0f, "B recorded SNR");
  /* A node that received nothing has no signal quality to report, rather than a
     plausible-looking zero. */
  CHECK(as.last_rssi_dbm == 0.0f, "A received nothing and reports no RSSI");
}

static void test_a_failing_radio_is_counted_and_does_not_wedge_the_queue(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");

  a.ctx.send_result       = -1; /* busy or absent, as radio.h reports it */
  const uint8_t payload[] = {0x01};
  (void)boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, false, payload,
                           sizeof(payload));
  boomlink_link_poll(&a.link);

  boomlink_link_stats_t s;
  boomlink_link_get_stats(&a.link, &s);
  CHECK(s.tx_failures == 1u, "the failure was counted, got %u", (unsigned)s.tx_failures);
  CHECK(s.tx_envelopes == 0u, "and not counted as a success");
  CHECK(s.tx_airtime_us == 0u, "a refused send radiated nothing");
  CHECK(fake_air_count(&air) == 0u, "nothing reached the air");

  /* The radio recovers. The queue must still work - a wedged pipeline after a
     transient send error would take the link down permanently. */
  a.ctx.send_result = 0;
  (void)boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, false, payload,
                           sizeof(payload));
  boomlink_link_poll(&a.link);
  boomlink_link_get_stats(&a.link, &s);
  CHECK(s.tx_envelopes == 1u, "the next frame went out once the radio recovered");
}

static void test_sequence_is_monotonic_despite_priority_reordering(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a, b;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  /* Queued lowest-priority first, so the queue must reorder them. Section 9.1
     assigns the sequence at DEQUEUE precisely so that reordering cannot make the
     on-air sequence non-monotonic - which would complicate the receiver's
     duplicate window, and section 9.8 says so outright. */
  const uint8_t low[]  = {0xA0};
  const uint8_t high[] = {0xB1};
  (void)boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_LOW, false, low, sizeof(low));
  (void)boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_HIGH, false, high,
                           sizeof(high));

  boomlink_link_poll(&a.link); /* sends the HIGH one */
  boomlink_link_poll(&a.link); /* then the LOW one */
  REQUIRE(fake_air_count(&air) == 2u, "both went out");

  boomlink_linkframe_header_t first, second;
  size_t                      ignored = 0u;
  const fake_transmission_t  *t0      = fake_air_transmission(&air, 0u);
  const fake_transmission_t  *t1      = fake_air_transmission(&air, 1u);
  REQUIRE(t0 != NULL && t1 != NULL, "both logged");
  REQUIRE(boomlink_linkframe_parse(t0->bytes, t0->len, BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
                                   &first, &ignored) == BOOMLINK_LINKFRAME_OK,
          "first parses");
  REQUIRE(boomlink_linkframe_parse(t1->bytes, t1->len, BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
                                   &second, &ignored) == BOOMLINK_LINKFRAME_OK,
          "second parses");

  CHECK(first.sequence == 1u, "the first frame on air carries sequence 1, got %u",
        (unsigned)first.sequence);
  CHECK(second.sequence == 2u, "and the second carries 2, got %u",
        (unsigned)second.sequence);
  CHECK(first.session_id == second.session_id, "both in the same session");
  CHECK(first.session_id == boomlink_link_session_id(&a.link), "and it is this node's");
}

int main(void) {
  test_init_refuses_an_unusable_configuration();
  test_a_unicast_frame_reaches_its_destination_and_nobody_else();
  test_broadcast_reaches_everyone_and_asks_for_no_ack();
  test_a_requested_ack_is_sent_and_is_the_frame_layers_ack();
  test_a_duplicate_is_not_delivered_twice_but_is_acknowledged_again();
  test_foreign_and_malformed_traffic_are_counted_apart();
  test_a_broadcast_asking_for_an_ack_is_never_acknowledged();
  test_an_unmatched_ack_is_counted_not_delivered();
  test_signal_quality_and_airtime_are_recorded();
  test_a_failing_radio_is_counted_and_does_not_wedge_the_queue();
  test_sequence_is_monotonic_despite_priority_reordering();
  BOOMLINK_TEST_REPORT("link_rx_test", 103);
}
