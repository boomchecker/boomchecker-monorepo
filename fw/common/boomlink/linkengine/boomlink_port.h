/**
 ******************************************************************************
 * @file    boomlink_port.h
 * @brief   The seam between BoomLink's link engine and everything below it:
 *          the radio, the clock and the random source.
 *
 *          boomlink.md section 15.2: "The link layer should depend on a small
 *          radio interface so a fake backend can test" ACK matching, ACK
 *          timeout, retry count, duplicate suppression, queue priority and
 *          randomized backoff bounds - none of which can be tested against real
 *          hardware in CI.
 *
 *          The engine cannot simply include the firmware's App/radio/radio.h.
 *          That header lives in fw/bom-stm32node (this package must stay
 *          host-buildable and knows nothing about that tree), it is a global
 *          singleton with no instance handle, and its own comment requires a
 *          replacement rather than reuse: "A future consumer (BoomLink, PR3)
 *          must replace this single-slot model with its own queue rather than
 *          add a second poller here."
 *
 *          Function pointers rather than link-time substitution, and one `ctx`
 *          rather than globals, for a specific reason: the interesting tests
 *          stand up TWO engines with a fake medium between them and exchange a
 *          real frame plus its real ACK. A link-time seam allows one
 *          implementation per binary and no per-node state, which would reduce
 *          every delivery test to simulating one side and asserting on what the
 *          other side "would" have done.
 ******************************************************************************
 */
#ifndef BOOMLINK_PORT_H
#define BOOMLINK_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The largest packet any port may declare, and the single place this number is
 * written down: RADIO_MAX_PAYLOAD on the target.
 *
 * It is a CEILING, not just a default, because the engine's buffers are
 * statically sized from it (agent rule 6): the RX staging buffer and the TX
 * queue's payload slots. A port claiming more would be promising a capacity the
 * engine has nowhere to put, and the failure would surface later as a truncated
 * receive rather than as a misconfiguration - which is why
 * boomlink_port_is_valid() refuses it, turning it into a bring-up failure.
 *
 * A port may of course declare LESS. That is an ordinary radio profile, not an
 * error, and the engine checks the real limit when it queues and sends.
 */
#define BOOMLINK_PORT_MAX_PACKET 255u

/**
 * Everything the engine needs from below it. Every function pointer receives
 * the same `ctx`: on the target that is unused (the callbacks forward to
 * radio.h's singleton and to the HAL tick), while a test's ctx is one object
 * holding the fake radio, the controllable clock and the seeded PRNG, which is
 * exactly the grouping a per-node fake wants.
 *
 * Every member is required. Pass this to boomlink_port_is_valid() before use -
 * a NULL callback would otherwise be a crash on the first packet, which on the
 * target means a hard fault in the field rather than a failure at bring-up.
 */
typedef struct {
  /**
   * Hand `len` bytes to the radio as one packet. Non-blocking: this returns as
   * soon as transmission has been ACCEPTED, not completed, matching radio.h's
   * radio_send() (and any sane LoRa driver - a full SF12 packet is seconds of
   * airtime, which the superloop cannot wait on).
   *
   * @return 0 if accepted, non-zero otherwise. The engine treats every non-zero
   *         value alike - the frame stays queued and is retried on a later tick,
   *         and the failure is counted - so a driver need not map its error
   *         codes onto anything. "Busy transmitting" and "hardware absent" are
   *         deliberately not distinguished: both mean try later, and radio.h
   *         itself already collapses them.
   */
  int (*send)(void *ctx, const uint8_t *frame, size_t len);

  /**
   * Non-blocking receive. If a packet has arrived since the last call, copies up
   * to `cap` bytes into `buf`, sets *out_len to the packet's TRUE length (which
   * may exceed `cap`, and the engine must treat that as a malformed
   * oversize packet rather than trusting the copy) and fills the signal quality
   * outputs, then returns true. Returns false when nothing new has arrived.
   *
   * out_rssi_dbm and out_snr_db are never NULL when called by the engine -
   * section 9.10 requires last RSSI and SNR in link statistics.
   */
  bool (*poll_rx)(void *ctx, uint8_t *buf, size_t cap, size_t *out_len,
                  float *out_rssi_dbm, float *out_snr_db);

  /**
   * Estimated time on air, in microseconds, for a `len`-byte packet under the
   * radio's ACTIVE PHY profile.
   *
   * On the port rather than computed in the engine, and this is a layering
   * decision worth stating. Section 9.6 requires the ACK timeout to be "derived
   * from/configured for the active radio profile rather than assuming one fixed
   * timeout for every spreading factor and packet size", and section 9.10
   * requires cumulative TX airtime for duty-cycle verification (section 6.1).
   * Both need the same LoRa symbol-time arithmetic, which depends on spreading
   * factor, bandwidth, coding rate, preamble length and the implicit/explicit
   * header choice. That knowledge belongs to the radio layer, and putting it here
   * keeps the engine free of PHY details - the same boundary that keeps Protobuf
   * out of the radio.
   *
   * Precisely, since an earlier version of this comment said the radio layer
   * "already has it": RadioLib can compute it (PhysicalLayer::getTimeOnAir(len),
   * in the vendored copy under fw/bom-stm32node/third_party/RadioLib), but
   * App/radio/radio.h does not expose it today. So implementing this callback on
   * the target means adding that accessor to the radio layer, not just forwarding
   * to something that is already there.
   *
   * A fake may return anything self-consistent; the engine only ever compares
   * and scales these values, never interprets them as physics.
   */
  uint32_t (*airtime_us)(void *ctx, size_t len);

  /**
   * Monotonic milliseconds. Only differences are ever used, so the epoch is
   * arbitrary and wrap is safe as long as the arithmetic is done on uint32_t
   * (see boomlink_elapsed_ms()). A 32-bit millisecond counter wraps every ~49.7
   * days, which is well within a deployment's uptime, so this is not
   * theoretical.
   */
  uint32_t (*now_ms)(void *ctx);

  /**
   * A uniformly distributed 32-bit value, for section 9.7's randomized retry
   * backoff and TX jitter. Quality requirements are low - this is collision
   * avoidance between a handful of nodes, not cryptography (section 14 keeps
   * that a separate concern) - but two nodes must not produce the SAME
   * sequence, or randomized backoff makes collisions worse rather than better
   * by keeping colliding nodes in lockstep. Seed it per node.
   */
  uint32_t (*random_u32)(void *ctx);

  /**
   * Largest packet the radio can move, BOOMLINK_PORT_MAX_PACKET on the target.
   * A frame longer than this is rejected before transmission per section 7.3
   * ("An oversized frame is rejected before transmission"), which is the check
   * the frame layer deliberately does not make - it has no radio dependency and
   * so cannot know this number.
   *
   * Bounded at BOTH ends by boomlink_port_is_valid(): at least a bare header,
   * at most BOOMLINK_PORT_MAX_PACKET. A smaller radio profile is legal and the
   * engine honours it - it is the ceiling for both queueing and receiving, so a
   * payload that would not fit is refused at boomlink_link_send() rather than
   * accepted and dropped later.
   */
  size_t max_packet;

  void *ctx;
} boomlink_port_t;

/**
 * Whether `port` is usable: non-NULL, every callback present, and a `max_packet`
 * between BOOMLINK_LINKFRAME_HEADER_SIZE (a port that cannot carry an empty
 * frame can carry nothing) and BOOMLINK_PORT_MAX_PACKET (see that constant).
 *
 * Checked once at init rather than defended against on every call, so the engine
 * body stays free of NULL tests. Returns false rather than aborting: on the
 * target a misconfigured port should leave the link down with the rest of the
 * firmware running, exactly as radio.h's own "never calls Error_Handler()"
 * policy does.
 */
bool boomlink_port_is_valid(const boomlink_port_t *port);

/**
 * Milliseconds from `earlier` to `now`, exact for any span shorter than 2^32 ms
 * (~49.7 days) including one that crosses a uint32_t wrap.
 *
 * Exists so no caller re-derives `now - start` and reaches for a signed or wider
 * type. Worth being precise about how that goes wrong, because the obvious
 * sabotage of this function is not a bug: for spans up to INT32_MAX a signed
 * two's-complement difference gives the SAME answer, wrap included - 0xFFFFFFFF
 * to 0x00000005 is 6 either way. It diverges only past ~24.8 days, where the
 * signed subtraction overflows (undefined behaviour) and a "clamp negatives to
 * zero" guard - the natural thing to write alongside it - reports no elapsed time
 * at all. A pending frame would then never time out. The unsigned form has no
 * such boundary inside its documented range, which is why the range is the full
 * 2^32 and not just "long enough for a timeout".
 */
uint32_t boomlink_elapsed_ms(uint32_t earlier, uint32_t now);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* BOOMLINK_PORT_H */
