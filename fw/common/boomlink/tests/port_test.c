/**
 ******************************************************************************
 * @file    port_test.c
 * @brief   Tests for the port seam (boomlink_port.h) and for the fake that
 *          implements it (fake_port.h).
 *
 *          The fake gets tested because every link engine test is built on it.
 *          A fake that delivers a node its own transmissions would make a
 *          duplicate-suppression test pass against an engine with no duplicate
 *          suppression at all; one whose clock advanced by itself would make an
 *          ACK-timeout test pass whatever the timeout was. Those bugs live in
 *          the harness, so no amount of care in the engine's tests would find
 *          them - the harness needs its own.
 ******************************************************************************
 */
#include <string.h>

#include "boomlink_linkframe.h"
#include "boomlink_port.h"
#include "c_test.h"
#include "fake_port.h"

BOOMLINK_TEST_STATE;

/* --- the seam's own contract ------------------------------------------------ */

static void test_port_validation_rejects_every_missing_piece(void) {
  fake_air_t      air;
  fake_port_ctx_t ctx;
  boomlink_port_t good;
  fake_air_init(&air);
  fake_port_init(&ctx, &air, 0u, 1u, &good);

  REQUIRE(boomlink_port_is_valid(&good), "the fake port should be valid to begin with");
  CHECK(!boomlink_port_is_valid(NULL), "a NULL port must be rejected");

  /* Each callback cleared in turn. Checked individually rather than as "some
     callback missing", because the interesting failure is a port that validates
     while one entry is NULL - the crash then happens on the first packet, which
     on the target is a hard fault in the field rather than a failure at
     bring-up. */
  struct {
    const char *name;
    size_t      offset;
  } const callbacks[] = {
      {"send", offsetof(boomlink_port_t, send)},
      {"poll_rx", offsetof(boomlink_port_t, poll_rx)},
      {"airtime_us", offsetof(boomlink_port_t, airtime_us)},
      {"now_ms", offsetof(boomlink_port_t, now_ms)},
      {"random_u32", offsetof(boomlink_port_t, random_u32)},
  };
  for (size_t i = 0; i < sizeof(callbacks) / sizeof(callbacks[0]); i++) {
    boomlink_port_t broken = good;
    /* Writing a function pointer through a byte offset needs a void* staging
       slot: a function pointer is not required to fit in an object pointer, so
       memset-ing the bytes is the portable way to say "cleared". */
    memset((unsigned char *)&broken + callbacks[i].offset, 0, sizeof(void (*)(void)));
    CHECK(!boomlink_port_is_valid(&broken), "a port missing %s must be rejected",
          callbacks[i].name);
  }

  boomlink_port_t tiny = good;
  tiny.max_packet      = BOOMLINK_LINKFRAME_HEADER_SIZE - 1u;
  CHECK(!boomlink_port_is_valid(&tiny),
        "max_packet %zu cannot hold a %u-byte header and must be rejected", tiny.max_packet,
        (unsigned)BOOMLINK_LINKFRAME_HEADER_SIZE);

  boomlink_port_t exact = good;
  exact.max_packet      = BOOMLINK_LINKFRAME_HEADER_SIZE;
  CHECK(boomlink_port_is_valid(&exact),
        "a radio that can carry exactly a header is usable - an empty DATA frame "
        "and every ACK are exactly that size");

  /* And the ceiling, which is not symmetry for its own sake: the engine stages
     received packets in a buffer sized from BOOMLINK_PORT_MAX_PACKET and offers
     poll_rx exactly max_packet bytes of it, so a port claiming more would have
     the engine hand out a capacity past the end of its own buffer. */
  boomlink_port_t at_ceiling = good;
  at_ceiling.max_packet      = BOOMLINK_PORT_MAX_PACKET;
  CHECK(boomlink_port_is_valid(&at_ceiling), "exactly the ceiling is usable");
  boomlink_port_t over_ceiling = good;
  over_ceiling.max_packet      = BOOMLINK_PORT_MAX_PACKET + 1u;
  CHECK(!boomlink_port_is_valid(&over_ceiling),
        "max_packet %zu is past the %u the engine can stage and must be rejected",
        over_ceiling.max_packet, (unsigned)BOOMLINK_PORT_MAX_PACKET);
}

static void test_elapsed_ms_survives_the_wrap(void) {
  CHECK(boomlink_elapsed_ms(1000u, 1500u) == 500u, "ordinary forward difference");
  CHECK(boomlink_elapsed_ms(7u, 7u) == 0u, "no time at all");
  /* The case the function exists for. A 32-bit millisecond counter wraps every
     ~49.7 days, which a deployment reaches, and an ACK timeout computed as a
     signed or widened difference across that boundary is either hugely negative
     or hugely positive - either way the pending frame never times out, or times
     out instantly and retries forever. */
  CHECK(boomlink_elapsed_ms(0xFFFFFFFFu, 5u) == 6u,
        "across the wrap: 0xFFFFFFFF -> 5 is 6 ms, got %u",
        (unsigned)boomlink_elapsed_ms(0xFFFFFFFFu, 5u));
  CHECK(boomlink_elapsed_ms(0xFFFFF000u, 0x00000FFFu) == 0x1FFFu,
        "a longer span across the wrap");

  /* Spans past INT32_MAX, which are the ONLY inputs that distinguish this from
     a signed difference. Verified: replacing the body with a signed subtraction
     plus a clamp-negatives-to-zero guard - the natural way to write it by hand -
     passes every case above, wrap included, because two's complement agrees with
     modular arithmetic up to INT32_MAX. It fails only here, and it fails in the
     dangerous direction: reporting 0 ms elapsed, so a pending frame never times
     out and the link stalls instead of retrying. Without these two cases that
     rewrite was invisible. */
  CHECK(boomlink_elapsed_ms(5u, 0xFFFFFFFFu) == 0xFFFFFFFAu,
        "a span longer than INT32_MAX must be exact, got %u",
        (unsigned)boomlink_elapsed_ms(5u, 0xFFFFFFFFu));
  CHECK(boomlink_elapsed_ms(0u, 0x90000000u) == 0x90000000u,
        "and one just past the signed boundary, got %u",
        (unsigned)boomlink_elapsed_ms(0u, 0x90000000u));
}

/* --- the fake's own behaviour ---------------------------------------------- */

static void test_a_node_never_hears_itself(void) {
  fake_air_t      air;
  fake_port_ctx_t a_ctx, b_ctx;
  boomlink_port_t a, b;
  fake_air_init(&air);
  fake_port_init(&a_ctx, &air, 0u, 1u, &a);
  fake_port_init(&b_ctx, &air, 1u, 2u, &b);

  const uint8_t frame[] = {0xB0, 0x11, 0x00, 0x00};
  REQUIRE(a.send(a.ctx, frame, sizeof(frame)) == 0, "the fake radio should accept a send");

  uint8_t buf[FAKE_PORT_MAX_PACKET];
  size_t  len   = 0u;
  float   rssi  = 0.0f;
  float   snr   = 0.0f;
  /* The property the whole two-node arrangement rests on. A fake that delivered
     a node its own frames would make duplicate suppression and ACK matching
     look like they worked while never involving a second node at all. */
  CHECK(!a.poll_rx(a.ctx, buf, sizeof(buf), &len, &rssi, &snr),
        "the sender must not receive its own transmission");
  REQUIRE(b.poll_rx(b.ctx, buf, sizeof(buf), &len, &rssi, &snr),
        "the other node must receive it");
  CHECK(len == sizeof(frame), "received %zu bytes, sent %zu", len, sizeof(frame));
  CHECK(memcmp(buf, frame, sizeof(frame)) == 0, "the bytes changed in flight");
  CHECK(rssi < 0.0f && snr > 0.0f, "signal quality must be reported: section 9.10 "
                                    "requires last RSSI and SNR in link statistics");
  CHECK(!b.poll_rx(b.ctx, buf, sizeof(buf), &len, &rssi, &snr),
        "a transmission must be delivered once, not on every poll");
}

static void test_a_swallowed_send_looks_successful_but_never_arrives(void) {
  fake_air_t      air;
  fake_port_ctx_t a_ctx, b_ctx;
  boomlink_port_t a, b;
  fake_air_init(&air);
  fake_port_init(&a_ctx, &air, 0u, 1u, &a);
  fake_port_init(&b_ctx, &air, 1u, 2u, &b);

  a_ctx.swallow_tx      = true;
  const uint8_t frame[] = {0xB0, 0x12, 0x00, 0x00};
  /* Lost packets MUST look like successful sends. An engine only retries on ACK
     timeout, so a fake that reported failure here would let a retry test pass
     against an engine that retries only on send errors - a completely different
     and much weaker behaviour than section 9.6 requires. */
  CHECK(a.send(a.ctx, frame, sizeof(frame)) == 0,
        "a swallowed (lost) transmission must still report success");
  CHECK(a_ctx.send_calls == 1u, "the send was counted");
  CHECK(a_ctx.sends_on_air == 0u, "nothing reached the air");
  CHECK(fake_air_count(&air) == 0u, "the air is empty");

  uint8_t buf[FAKE_PORT_MAX_PACKET];
  size_t  len  = 0u;
  float   rssi = 0.0f, snr = 0.0f;
  CHECK(!b.poll_rx(b.ctx, buf, sizeof(buf), &len, &rssi, &snr),
        "a swallowed transmission must not be received");

  a_ctx.send_result = -7;
  CHECK(a.send(a.ctx, frame, sizeof(frame)) == -7,
        "an injected send failure must be reported verbatim");
  CHECK(a_ctx.sends_on_air == 0u, "a failed send puts nothing on the air");
}

static void test_the_clock_only_moves_when_moved(void) {
  fake_air_t      air;
  fake_port_ctx_t ctx;
  boomlink_port_t port;
  fake_air_init(&air);
  fake_port_init(&ctx, &air, 0u, 1u, &port);

  const uint32_t first = port.now_ms(port.ctx);
  for (int i = 0; i < 1000; i++) {
    /* Real work between reads, so a clock that tracked anything real - wall
       time, a call counter - would have moved. */
    (void)port.random_u32(port.ctx);
  }
  CHECK(port.now_ms(port.ctx) == first,
        "the clock advanced on its own, from %u to %u: every timing test built on "
        "this would then be measuring the machine rather than the engine",
        (unsigned)first, (unsigned)port.now_ms(port.ctx));

  /* Several different steps, and they accumulate. A single advance cannot tell
     "adds delta" from "sets the clock to delta" or from "adds a constant": with
     one 250 ms step from zero, all three give 250. The retry tests are built on
     repeated advances of different sizes, so that distinction is exactly the one
     that has to hold. */
  const uint32_t steps[] = {250u, 1u, 0u, 60000u};
  uint32_t       expected = 0u;
  for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
    fake_port_advance_ms(&ctx, steps[i]);
    expected += steps[i];
    CHECK(boomlink_elapsed_ms(first, port.now_ms(port.ctx)) == expected,
          "after advancing by %u the total elapsed must be %u, got %u",
          (unsigned)steps[i], (unsigned)expected,
          (unsigned)boomlink_elapsed_ms(first, port.now_ms(port.ctx)));
  }
}

static void test_airtime_scales_with_length(void) {
  fake_air_t      air;
  fake_port_ctx_t ctx;
  boomlink_port_t port;
  fake_air_init(&air);
  fake_port_init(&ctx, &air, 0u, 1u, &port);

  const uint32_t header = port.airtime_us(port.ctx, BOOMLINK_LINKFRAME_HEADER_SIZE);
  const uint32_t bigger = port.airtime_us(port.ctx, BOOMLINK_LINKFRAME_HEADER_SIZE + 100u);
  CHECK(header > 0u, "a header takes non-zero airtime, or a derived timeout is zero");
  CHECK(bigger > header, "a longer packet must take longer: %u vs %u us",
        (unsigned)bigger, (unsigned)header);
  CHECK(port.airtime_us(port.ctx, 0u) == 0u, "an empty packet is the zero point");
}

static void test_nodes_seeded_differently_do_not_march_in_lockstep(void) {
  fake_air_t      air;
  fake_port_ctx_t a_ctx, b_ctx;
  boomlink_port_t a, b;
  fake_air_init(&air);
  fake_port_init(&a_ctx, &air, 0u, 0xC0FFEEu, &a);
  fake_port_init(&b_ctx, &air, 1u, 0xBADCAFEu, &b);

  /* Section 9.7's randomized backoff exists to break up simultaneous
     transmissions after a shared trigger - several nodes detecting the same
     gunshot. Two nodes drawing the SAME sequence keeps them in lockstep and
     makes collisions worse rather than better, so a scenario that seeds them
     identically would be testing the opposite of the intended behaviour. */
  int identical = 0;
  for (int i = 0; i < 16; i++) {
    if (a.random_u32(a.ctx) == b.random_u32(b.ctx)) {
      identical++;
    }
  }
  CHECK(identical == 0, "%d of 16 draws were identical across two differently seeded nodes",
        identical);

  /* And the same seed must reproduce exactly, or a backoff-bounds failure could
     not be re-run. */
  fake_port_ctx_t r1, r2;
  boomlink_port_t p1, p2;
  fake_port_init(&r1, &air, 2u, 42u, &p1);
  fake_port_init(&r2, &air, 3u, 42u, &p2);
  for (int i = 0; i < 16; i++) {
    CHECK(p1.random_u32(p1.ctx) == p2.random_u32(p2.ctx),
          "draw %d differed between two identically seeded generators", i);
  }

  /* A zero seed must not stick: xorshift32 seeded with 0 returns 0 forever, and
     every "random" backoff would be the same value. */
  fake_port_ctx_t z;
  boomlink_port_t pz;
  fake_port_init(&z, &air, 4u, 0u, &pz);
  CHECK(pz.random_u32(pz.ctx) != 0u, "a zero seed must not produce a stuck generator");
  CHECK(pz.random_u32(pz.ctx) != 0u, "still stuck on the second draw");
}

static void test_an_oversize_packet_reports_its_true_length(void) {
  fake_air_t      air;
  fake_port_ctx_t b_ctx;
  boomlink_port_t b;
  fake_air_init(&air);
  fake_port_init(&b_ctx, &air, 1u, 2u, &b);

  /* Injected as if from another node, since no engine would send this. The
     engine must be able to tell a too-long packet from a valid short one, which
     it can only do if the fake reports the length as transmitted rather than the
     length that fitted - otherwise an oversize packet arrives looking like a
     perfectly ordinary truncated frame. */
  uint8_t big[FAKE_PORT_MAX_PACKET];
  memset(big, 0xAB, sizeof(big));
  /* Longer than the log record itself can hold, which is the case that actually
     pins this. At exactly FAKE_PORT_MAX_PACKET the record's stored length and the
     transmitted length are the SAME number, so a fake reporting the stored length
     instead of the transmitted one would pass - verified. One byte more separates
     them: the record keeps 255, the wire carried 256, and only the second is a
     length the engine can recognise as impossible. */
  const size_t wire = sizeof(big) + 1u;
  fake_air_inject(&air, 0u, big, wire);

  uint8_t small_buf[BOOMLINK_LINKFRAME_HEADER_SIZE];
  size_t  len  = 0u;
  float   rssi = 0.0f, snr = 0.0f;
  REQUIRE(b.poll_rx(b.ctx, small_buf, sizeof(small_buf), &len, &rssi, &snr),
          "the injected packet should be received");
  CHECK(len == wire, "reported length %zu, transmitted %zu - the engine cannot "
                     "detect an oversize packet if the length is clamped",
        len, wire);

  /* The truncation is real too: the buffer offered was 20 bytes, and nothing may
     have been written past it. A fake that copied the whole packet would make
     every oversize test in the engine pass against a genuine overrun. */
  const fake_transmission_t *record = fake_air_transmission(&air, 0u);
  REQUIRE(record != NULL, "the oversize packet was logged");
  CHECK(record->len == FAKE_PORT_MAX_PACKET,
        "the record keeps what fits (%zu), not what was sent", record->len);
  CHECK(record->wire_len == wire, "and remembers the true length (%zu)",
        record->wire_len);
}

static void test_the_air_reports_its_own_overflow(void) {
  fake_air_t      air;
  fake_port_ctx_t ctx;
  boomlink_port_t port;
  fake_air_init(&air);
  fake_port_init(&ctx, &air, 0u, 1u, &port);
  CHECK(fake_air_ok(&air), "a fresh air has not overflowed");

  const uint8_t frame[] = {0xB0};
  for (size_t i = 0; i < FAKE_PORT_MAX_TRANSMISSIONS; i++) {
    (void)port.send(port.ctx, frame, sizeof(frame));
  }
  CHECK(fake_air_ok(&air), "exactly at capacity is not an overflow");
  CHECK(fake_air_count(&air) == FAKE_PORT_MAX_TRANSMISSIONS, "every send was logged");

  (void)port.send(port.ctx, frame, sizeof(frame));
  /* A scenario that overflows the log has assertions about transmissions the log
     no longer holds, so it must fail rather than quietly pass on a prefix. */
  CHECK(!fake_air_ok(&air), "one past capacity must be reported as an overflow");
  CHECK(fake_air_transmission(&air, FAKE_PORT_MAX_TRANSMISSIONS) == NULL,
        "reading past the log must return NULL, not a stale record");
}

int main(void) {
  test_port_validation_rejects_every_missing_piece();
  test_elapsed_ms_survives_the_wrap();
  test_a_node_never_hears_itself();
  test_a_swallowed_send_looks_successful_but_never_arrives();
  test_the_clock_only_moves_when_moved();
  test_airtime_scales_with_length();
  test_nodes_seeded_differently_do_not_march_in_lockstep();
  test_an_oversize_packet_reports_its_true_length();
  test_the_air_reports_its_own_overflow();
  BOOMLINK_TEST_REPORT("port_test", 68);
}
