/**
 ******************************************************************************
 * @file    link_rx_test.c
 * @brief   The link engine's RX pipeline (boomlink.md section 9.2), and the
 *          end-to-end delivery that pipeline exists for.
 *
 *          Where delivery is the subject, a scenario runs TWO engines on one fake
 *          medium, so a frame is built by one and validated by the other. That is
 *          not ceremony: a test that constructed the expected frame itself and fed
 *          it in would agree with a misreading of section 7.3 as happily as the
 *          code does.
 *
 *          The rejection scenarios are the other way round on purpose, and there
 *          are several: no compliant engine emits a frame from the broadcast
 *          address, or an ACK addressed to a third party, so those have to be
 *          forged with the frame layer's encoder and injected. An earlier version
 *          of this comment claimed EVERY scenario ran two engines; it never did,
 *          and could not - a receiver-side guard can only be reached by traffic no
 *          sender would produce.
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

/**
 * Bring up a node whose radio carries at most `max_packet` bytes. Zero means the
 * fake's full capacity, which is what every scenario but the reduced-profile one
 * wants.
 */
static bool node_up_with_max_packet(node_t *n, fake_air_t *air, uint8_t index,
                                    uint32_t node_id, size_t max_packet) {
  memset(n, 0, sizeof(*n));
  fake_port_init(&n->ctx, air, index, 0x1000u + node_id, &n->port);
  if (max_packet != 0u) {
    n->port.max_packet = max_packet;
  }
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

static bool node_up(node_t *n, fake_air_t *air, uint8_t index, uint32_t node_id) {
  return node_up_with_max_packet(n, air, index, node_id, 0u);
}

/**
 * Every scenario ends here. The air's overflow flag is recorded rather than
 * raised from inside a callback (see fake_port.c), so unless something checks it
 * a scenario that transmitted more than the log holds would assert happily
 * against a truncated prefix. Nothing did until this existed.
 */
static void scenario_end(const fake_air_t *air) {
  CHECK(fake_air_ok(air),
        "the transmission log overflowed: this scenario's assertions were made "
        "against a truncated prefix of what was actually sent");
}

/** Encode `header` and inject it as if `sender` had transmitted it. */
static void inject_header(fake_air_t *air, uint8_t sender,
                          const boomlink_linkframe_header_t *header) {
  uint8_t bytes[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(header, bytes);
  fake_air_inject(air, sender, bytes, sizeof(bytes));
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

  /* A port claiming more than the engine can stage. Accepting it would offer
     poll_rx a capacity larger than the RX buffer, which is a buffer overrun on
     the first oversized packet rather than a misconfiguration. */
  boomlink_port_t too_big = port;
  too_big.max_packet      = BOOMLINK_PORT_MAX_PACKET + 1u;
  CHECK(!boomlink_link_init(&n.link, &base, &too_big, 1u),
        "a port claiming %zu bytes must be refused: the engine stages at most %u",
        too_big.max_packet, (unsigned)BOOMLINK_PORT_MAX_PACKET);

  /* Section 9.3: a session that was never assigned. The value that arrives here
     by accident is 0 - an unseeded PRNG, a zeroed config struct - and a node that
     reuses one session across reboots goes deaf to its peer, since every frame
     after the reboot replays sequences the peer already accepted. */
  CHECK(!boomlink_link_init(&n.link, &base, &port, 0u),
        "session_id 0 means nothing was assigned and must be refused");
  CHECK(boomlink_link_init(&n.link, &base, &port, 1u),
        "and any other value is accepted, so the check is not just 'refuse all'");
  scenario_end(&air);
}

static void test_the_diagnostics_tolerate_a_null_link(void) {
  /* These three are reached from a CLI or a telemetry builder, not from the
     superloop. On the target a wrong pointer there should cost a missing reading,
     not a hard fault in the field - which is radio.h's own stated policy. */
  boomlink_link_stats_t s;
  memset(&s, 0xA5, sizeof(s));
  boomlink_link_get_stats(NULL, &s);
  CHECK(s.tx_envelopes == 0u && s.rx_envelopes == 0u,
        "a NULL link must zero the output rather than leave the caller's stale bytes");

  fake_air_t air;
  fake_air_init(&air);
  node_t a;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  boomlink_link_get_stats(&a.link, NULL); /* must not dereference */
  boomlink_link_reset_stats(NULL);
  CHECK(boomlink_link_session_id(NULL) == 0u,
        "a NULL link reports the 'never assigned' session, got %u",
        (unsigned)boomlink_link_session_id(NULL));
  CHECK(boomlink_link_session_id(&a.link) != 0u, "and a real one reports its own");
  scenario_end(&air);
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
                           sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
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
  scenario_end(&air);
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
                           payload, sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
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
  scenario_end(&air);
}

static void test_a_requested_ack_is_sent_and_is_the_frame_layers_ack(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a, b;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  const uint8_t payload[] = {0x42};
  CHECK(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, true, payload,
                           sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
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
     the engine to section 9.5 without restating the mapping here.
     One thing this cannot distinguish, and it is worth saying rather than
     implying otherwise: NODE_B is both the acknowledging node's own ID and the
     received frame's destination_id, so an engine passing `received->
     destination_id` instead of its own configured ID would produce the same
     bytes. No test can separate them, because only an exact-destination unicast
     frame is ever acknowledged - a broadcast frame, the one case where the two
     differ, is never answered at all (see the forged-broadcast scenario). The
     substitution is unobservable because it is currently equivalent, not because
     it is covered. */
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
  scenario_end(&air);
}

static void test_a_duplicate_is_not_delivered_twice_but_is_acknowledged_again(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a, b;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  const uint8_t payload[] = {0x77};
  CHECK(boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, true, payload,
                           sizeof(payload)) == BOOMLINK_LINK_SEND_OK,
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
  scenario_end(&air);
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
  /* The third one. Counted apart from malformed on purpose: a flood of these
     means a foreign transmitter or a driver fault, not corrupt traffic - and
     until this assertion existed nothing checked the counter at all, so the whole
     oversize branch could have been deleted with the suite green. */
  CHECK(s.rx_oversize == 1u, "the oversize packet, got %u", (unsigned)s.rx_oversize);
  CHECK(s.rx_envelopes == 0u, "nothing was delivered");
  CHECK(b.rx.calls == 0u, "and the callback was never reached");
  CHECK(s.ack_sent == 0u, "none of it was acknowledged");
  scenario_end(&air);
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
  scenario_end(&air);
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
  scenario_end(&air);
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
  scenario_end(&air);
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
  scenario_end(&air);
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
  scenario_end(&air);
}

static void test_a_frame_from_an_impossible_source_is_never_accepted(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t b;
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  /* Three source addresses no peer can legitimately have, each addressed
     correctly to B and each asking for an ACK. All must be dropped, and none may
     be answered: an ACK to the unconfigured or broadcast address is a frame no
     receiver can use, and an ACK to our OWN address is this node acknowledging
     itself.
     The third is the one worth the test. It is what a reflection off a repeater,
     a second board flashed with the same node ID, or a spoof looks like - and
     accepting it also files the frame in our duplicate cache under our own key,
     so this node's own later traffic could be suppressed as a duplicate of it. */
  const uint32_t impossible[] = {BOOMLINK_ADDR_INVALID, BOOMLINK_ADDR_BROADCAST, NODE_B};
  for (size_t i = 0; i < sizeof(impossible) / sizeof(impossible[0]); i++) {
    boomlink_linkframe_header_t h;
    boomlink_linkframe_header_init(&h);
    h.magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT;
    h.destination_id = NODE_B;
    h.source_id      = impossible[i];
    h.session_id     = 0x900u + (uint32_t)i;
    h.sequence       = 1u;
    h.ack_requested  = true;
    inject_header(&air, 0u, &h);
  }
  boomlink_link_poll(&b.link);

  boomlink_link_stats_t s;
  boomlink_link_get_stats(&b.link, &s);
  CHECK(s.rx_invalid_source == 3u, "all three were rejected on their source, got %u",
        (unsigned)s.rx_invalid_source);
  CHECK(b.rx.calls == 0u, "and none reached the application");
  CHECK(s.rx_envelopes == 0u, "nor was counted as delivered");
  CHECK(s.ack_sent == 0u, "and none was acknowledged");
  CHECK(fake_air_count(&air) == 3u, "so nothing new reached the air, got %zu",
        fake_air_count(&air));
  /* Not folded into malformed or wrong-destination: both of those were correct
     here, which is exactly why a counter of their own was needed. */
  CHECK(s.rx_malformed == 0u, "these are well-formed frames");
  CHECK(s.rx_other_destination == 0u, "addressed to us, precisely");

  /* And the control: the same shape from a real peer IS accepted, so the check
     rejects three specific addresses rather than everything. */
  boomlink_linkframe_header_t good;
  boomlink_linkframe_header_init(&good);
  good.magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT;
  good.destination_id = NODE_B;
  good.source_id      = NODE_A;
  good.session_id     = 0x910u;
  good.sequence       = 1u;
  good.ack_requested  = true;
  inject_header(&air, 0u, &good);
  boomlink_link_poll(&b.link);
  boomlink_link_get_stats(&b.link, &s);
  CHECK(s.rx_envelopes == 1u, "a frame from a real peer is delivered");
  CHECK(s.ack_sent == 1u, "and acknowledged");
  scenario_end(&air);
}

static void test_an_ack_for_someone_else_is_not_an_unmatched_ack(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t b;
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  /* An ACK between two OTHER nodes. On a shared medium this arrives constantly,
     and it is not a link event: ack_unmatched means "an ACK meant for me that
     acknowledges nothing I am waiting for", which is a late or forged ACK worth
     looking at. Counting overheard traffic there buries the interesting case in
     an ever-growing count of the ordinary one. */
  boomlink_linkframe_header_t elsewhere;
  boomlink_linkframe_header_init(&elsewhere);
  elsewhere.magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT;
  elsewhere.frame_type     = BOOMLINK_FRAME_TYPE_ACK;
  elsewhere.destination_id = NODE_C;
  elsewhere.source_id      = NODE_A;
  elsewhere.session_id     = 0x11u;
  elsewhere.sequence       = 7u;
  inject_header(&air, 0u, &elsewhere);

  /* A broadcast-addressed ACK. is_for_node() would accept it - broadcast is "for
     everyone" - but section 9.5's matching rule requires an ACK's destination to
     equal the receiving node's own ID, which 0xFFFFFFFF never does. So the ACK
     path compares exact equality rather than reusing the acceptance rule, and
     this vector is what tells the two apart. */
  boomlink_linkframe_header_t broadcast_ack = elsewhere;
  broadcast_ack.destination_id             = BOOMLINK_ADDR_BROADCAST;
  broadcast_ack.sequence                   = 8u;
  inject_header(&air, 0u, &broadcast_ack);

  boomlink_link_poll(&b.link);

  boomlink_link_stats_t s;
  boomlink_link_get_stats(&b.link, &s);
  CHECK(s.ack_unmatched == 0u,
        "neither ACK was addressed to this node, so neither is an unmatched ACK "
        "for it: got %u",
        (unsigned)s.ack_unmatched);
  CHECK(s.rx_other_destination == 2u, "both counted as another node's traffic, got %u",
        (unsigned)s.rx_other_destination);
  CHECK(s.rx_envelopes == 0u, "an ACK is never application data");
  CHECK(b.rx.calls == 0u, "and never reaches the callback");

  /* The control: addressed to US, it IS an unmatched ACK - so the two paths are
     distinguished rather than one of them being dead. */
  boomlink_linkframe_header_t for_us = elsewhere;
  for_us.destination_id             = NODE_B;
  for_us.sequence                   = 9u;
  inject_header(&air, 0u, &for_us);
  boomlink_link_poll(&b.link);
  boomlink_link_get_stats(&b.link, &s);
  CHECK(s.ack_unmatched == 1u, "an ACK addressed to us with nothing pending, got %u",
        (unsigned)s.ack_unmatched);
  CHECK(s.rx_other_destination == 2u, "and it was not counted as someone else's");
  scenario_end(&air);
}

static void test_a_reduced_radio_profile_bounds_what_is_accepted(void) {
  fake_air_t air;
  fake_air_init(&air);
  /* A radio profile that carries far less than the 255 bytes the type system
     allows - a high spreading factor, say. The engine's staging buffer is still
     the full size, so a check written against the BUFFER would accept everything
     below and this scenario would be indistinguishable from the default one.
     Verified: comparing against sizeof(rx_buffer) leaves the oversize assertion
     below failing and nothing else. */
  const size_t profile = 64u;
  node_t       b;
  REQUIRE(node_up_with_max_packet(&b, &air, 1u, NODE_B, profile), "B came up");

  uint8_t frame[BOOMLINK_PORT_MAX_PACKET];
  boomlink_linkframe_header_t h;
  boomlink_linkframe_header_init(&h);
  h.magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT;
  h.destination_id = NODE_B;
  h.source_id      = NODE_A;
  h.session_id     = 0x700u;
  h.sequence       = 1u;
  boomlink_linkframe_encode(&h, frame);
  memset(&frame[BOOMLINK_LINKFRAME_HEADER_SIZE], 0x5Cu,
         sizeof(frame) - BOOMLINK_LINKFRAME_HEADER_SIZE);

  /* One byte past what this radio can carry: a perfectly well-formed frame that
     this node's radio could not have produced or fully received. */
  fake_air_inject(&air, 0u, frame, profile + 1u);
  boomlink_link_poll(&b.link);
  boomlink_link_stats_t s;
  boomlink_link_get_stats(&b.link, &s);
  CHECK(s.rx_oversize == 1u, "%zu bytes on a %zu-byte profile is oversize, got %u",
        profile + 1u, profile, (unsigned)s.rx_oversize);
  CHECK(s.rx_envelopes == 0u, "and nothing was delivered from it");
  CHECK(b.rx.calls == 0u, "the callback was never reached");

  /* Exactly at the profile's limit must still work, or the check is off by one in
     the other direction and the largest legal frame can never be received. */
  h.sequence = 2u;
  boomlink_linkframe_encode(&h, frame);
  fake_air_inject(&air, 0u, frame, profile);
  boomlink_link_poll(&b.link);
  boomlink_link_get_stats(&b.link, &s);
  CHECK(s.rx_oversize == 1u, "a frame of exactly %zu bytes is not oversize", profile);
  CHECK(s.rx_envelopes == 1u, "and it was delivered, got %u", (unsigned)s.rx_envelopes);
  CHECK(b.rx.last_len == profile - BOOMLINK_LINKFRAME_HEADER_SIZE,
        "with its whole payload: %zu bytes, expected %zu", b.rx.last_len,
        profile - BOOMLINK_LINKFRAME_HEADER_SIZE);
  scenario_end(&air);
}

static void test_a_byte_corrupted_in_flight_is_classified_not_delivered(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a, b;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  /* Corruption on a real transmission from a real engine, rather than a header
     assembled by the test: the bytes that get damaged are the ones the engine
     actually emits.

     Damage inside the PAYLOAD first, and it must NOT be rejected. Section 9.2 puts
     a failed Protobuf decode at the BoomProtocol layer, counted separately, so
     this layer hands the bytes over rather than judging them - a link that dropped
     frames on payload content would be silently filtering the application. This
     frame asks for no ACK, so the pipeline is free again afterwards (section 9.6
     holds the queue while one is pending, which is why the order here matters). */
  const uint8_t payload[]  = {0x11, 0x22};
  a.ctx.corrupt_byte_index = (int)BOOMLINK_LINKFRAME_HEADER_SIZE;
  (void)boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, false, payload,
                           sizeof(payload));
  boomlink_link_poll(&a.link);
  CHECK(a.ctx.corrupt_byte_index < 0, "the injector is one-shot");
  boomlink_link_poll(&b.link);

  boomlink_link_stats_t bs;
  boomlink_link_get_stats(&b.link, &bs);
  CHECK(bs.rx_envelopes == 1u, "a damaged payload is still delivered, got %u",
        (unsigned)bs.rx_envelopes);
  CHECK(b.rx.last_payload[0] == (uint8_t)(payload[0] ^ 0xFFu),
        "with the damage intact: got %#04x", b.rx.last_payload[0]);

  /* Byte 0 is the magic (section 7.3's layout), so this is what a bit error in the
     first symbol looks like - and section 7.3 requires it dropped and counted
     before any further processing. This one DOES ask for an ACK, so the silence
     that follows is a real refusal rather than a frame that never wanted one. */
  a.ctx.corrupt_byte_index = 0;
  (void)boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, true, payload,
                           sizeof(payload));
  boomlink_link_poll(&a.link);
  REQUIRE(fake_air_count(&air) == 2u, "the damaged frame reached the air");
  boomlink_link_poll(&b.link);
  boomlink_link_get_stats(&b.link, &bs);
  CHECK(bs.rx_rejected_magic_or_version == 1u,
        "a damaged magic byte is a foreign/unreadable frame, got %u",
        (unsigned)bs.rx_rejected_magic_or_version);
  CHECK(bs.rx_envelopes == 1u, "nothing further was delivered");
  CHECK(bs.ack_sent == 0u, "and nothing was acknowledged - the sender will retry");
  CHECK(fake_air_count(&air) == 2u, "no ACK reached the air");
  CHECK(boomlink_link_tx_state(&a.link) == BOOMLINK_TX_STATE_WAIT_ACK,
        "so A is still waiting for an ACK that will never come");
  scenario_end(&air);
}

static void test_the_duplicate_key_is_source_session_and_sequence(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t b;
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  /* Section 9.3 names the identity of a received packet as (source_id,
     session_id, sequence). Each of the three vectors below differs from the first
     frame in exactly ONE of those fields, so a cache that keyed on only two of
     them - or on the sequence alone - suppresses a frame it must deliver. That is
     data loss, and silent: the sender is ACKed and never learns. Without these
     the engine's tests pinned no key at all, since every frame in them came from
     one peer in one session. */
  boomlink_linkframe_header_t h;
  boomlink_linkframe_header_init(&h);
  h.magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT;
  h.destination_id = NODE_B;
  h.source_id      = NODE_A;
  h.session_id     = 0x111u;
  h.sequence       = 5u;
  inject_header(&air, 0u, &h);

  boomlink_linkframe_header_t other_source = h;
  other_source.source_id                   = NODE_C;
  inject_header(&air, 2u, &other_source);

  boomlink_linkframe_header_t other_sequence = h;
  other_sequence.sequence                    = 6u;
  inject_header(&air, 0u, &other_sequence);

  boomlink_link_poll(&b.link);
  boomlink_link_stats_t s;
  boomlink_link_get_stats(&b.link, &s);
  CHECK(s.rx_envelopes == 3u,
        "a differing source and a differing sequence are different frames, got %u",
        (unsigned)s.rx_envelopes);
  CHECK(s.rx_duplicates == 0u, "neither is a duplicate of the first, got %u",
        (unsigned)s.rx_duplicates);

  /* The exact triple repeated IS a duplicate, so the key is not simply ignored.
     This repeats the FIRST frame, whose entry has since been overtaken by a
     higher sequence, so it is the reordering window (section 9.4) that has to
     remember it and not just the highest-sequence value. */
  inject_header(&air, 0u, &h);
  boomlink_link_poll(&b.link);
  boomlink_link_get_stats(&b.link, &s);
  CHECK(s.rx_duplicates == 1u, "the exact triple repeated is a duplicate, got %u",
        (unsigned)s.rx_duplicates);
  CHECK(s.rx_envelopes == 3u, "and was not delivered again");

  /* The session field, kept last on purpose. A frame from the SAME source in a
     DIFFERENT session is a peer that rebooted (section 9.3), and the cache reuses
     that peer's slot rather than adding a second one - so this deliberately
     discards the window built above, which is why it cannot be interleaved with
     the vectors before it.
     That reuse has a cost worth naming: one frame from a stale session resets what
     the current session's window remembers, so a straggler that was in flight
     across the peer's reboot can make one already-delivered frame deliverable
     again. Section 9.4 already accepts exactly that outcome for LRU eviction ("a
     very stale retransmission from an evicted source may be delivered twice"), and
     the alternative - letting one rebooting peer occupy two slots - shrinks the
     table for everyone else. */
  boomlink_linkframe_header_t other_session = h;
  other_session.session_id                  = 0x222u;
  inject_header(&air, 0u, &other_session);
  boomlink_link_poll(&b.link);
  boomlink_link_get_stats(&b.link, &s);
  CHECK(s.rx_envelopes == 4u, "the same sequence in a new session is a new frame, got %u",
        (unsigned)s.rx_envelopes);
  CHECK(s.rx_duplicates == 1u, "and not a duplicate of the old session's, got %u",
        (unsigned)s.rx_duplicates);

  /* And the new session tracks its own duplicates from there. */
  inject_header(&air, 0u, &other_session);
  boomlink_link_poll(&b.link);
  boomlink_link_get_stats(&b.link, &s);
  CHECK(s.rx_duplicates == 2u, "the new session suppresses its own repeat, got %u",
        (unsigned)s.rx_duplicates);
  CHECK(s.rx_envelopes == 4u, "delivering nothing further");
  scenario_end(&air);
}

static void test_resetting_the_statistics_does_not_reset_the_link(void) {
  fake_air_t air;
  fake_air_init(&air);
  node_t a, b;
  REQUIRE(node_up(&a, &air, 0u, NODE_A), "A came up");
  REQUIRE(node_up(&b, &air, 1u, NODE_B), "B came up");

  const uint8_t payload[] = {0x5Eu};
  (void)boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, false, payload,
                           sizeof(payload));
  boomlink_link_poll(&a.link);
  boomlink_link_poll(&b.link);
  REQUIRE(fake_air_count(&air) == 1u, "one frame on the air");
  const fake_transmission_t *tx = fake_air_transmission(&air, 0u);
  REQUIRE(tx != NULL, "logged");
  uint8_t      copy[BOOMLINK_PORT_MAX_PACKET];
  const size_t copy_len = tx->len;
  memcpy(copy, tx->bytes, copy_len);

  const uint32_t session_before = boomlink_link_session_id(&a.link);
  boomlink_link_reset_stats(&a.link);
  boomlink_link_reset_stats(&b.link);

  boomlink_link_stats_t as, bs;
  boomlink_link_get_stats(&a.link, &as);
  boomlink_link_get_stats(&b.link, &bs);
  CHECK(as.tx_envelopes == 0u && as.tx_airtime_us == 0u, "A's counters were zeroed");
  CHECK(bs.rx_envelopes == 0u, "and B's");

  /* Everything below is state a memset of the whole engine would have destroyed,
     and each destruction is a link that looks alive and interoperates with
     nothing. Verified: replacing the body with memset(link, 0, sizeof(*link))
     leaves the counter assertions above passing and fails every one of these. */
  CHECK(boomlink_link_session_id(&a.link) == session_before,
        "the session must survive: a new one mid-conversation would make the peer "
        "treat this node as rebooted");
  CHECK(session_before != 0u, "and it was a real session to begin with");

  /* B's duplicate cache must still hold the frame it already delivered. */
  fake_air_inject(&air, 0u, copy, copy_len);
  boomlink_link_poll(&b.link);
  boomlink_link_get_stats(&b.link, &bs);
  CHECK(bs.rx_duplicates == 1u,
        "the duplicate cache must survive a statistics reset, got %u duplicates",
        (unsigned)bs.rx_duplicates);
  CHECK(bs.rx_envelopes == 0u, "so the frame was not delivered a second time");
  CHECK(b.rx.calls == 1u, "the application still saw it exactly once, got %u",
        b.rx.calls);

  /* And A's sequence must continue rather than restart: a restarted sequence
     inside a live session is a replay of numbers the peer has already accepted,
     and it would suppress this node's traffic until the entry aged out. */
  (void)boomlink_link_send(&a.link, NODE_B, BOOMLINK_TXPRIO_NORMAL, false, payload,
                           sizeof(payload));
  boomlink_link_poll(&a.link);
  const fake_transmission_t *second = fake_air_transmission(&air, fake_air_count(&air) - 1u);
  REQUIRE(second != NULL, "the second frame was logged");
  boomlink_linkframe_header_t parsed;
  size_t                      ignored = 0u;
  REQUIRE(boomlink_linkframe_parse(second->bytes, second->len,
                                   BOOMLINK_LINKFRAME_MAGIC_DEFAULT, &parsed,
                                   &ignored) == BOOMLINK_LINKFRAME_OK,
          "it parses");
  CHECK(parsed.sequence == 2u,
        "the sequence must continue past the reset, got %u - a restart here would "
        "replay numbers the peer has already accepted",
        (unsigned)parsed.sequence);
  CHECK(parsed.session_id == session_before, "in the same session");
  scenario_end(&air);
}

int main(void) {
  test_init_refuses_an_unusable_configuration();
  test_the_diagnostics_tolerate_a_null_link();
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
  test_a_frame_from_an_impossible_source_is_never_accepted();
  test_an_ack_for_someone_else_is_not_an_unmatched_ack();
  test_a_reduced_radio_profile_bounds_what_is_accepted();
  test_a_byte_corrupted_in_flight_is_classified_not_delivered();
  test_the_duplicate_key_is_source_session_and_sequence();
  test_resetting_the_statistics_does_not_reset_the_link();
  BOOMLINK_TEST_REPORT("link_rx_test", 187);
}
