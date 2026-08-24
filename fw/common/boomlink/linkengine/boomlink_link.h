/**
 ******************************************************************************
 * @file    boomlink_link.h
 * @brief   BoomLink's link engine (boomlink.md section 9): the stateful half of
 *          the link layer.
 *
 *          Turns the stateless frame codec (boomlink_linkframe.h) into an
 *          addressed, acknowledged, deduplicated link: section 9.1's TX
 *          pipeline, 9.2's RX pipeline, 9.3's session/sequence, 9.4's duplicate
 *          suppression, 9.5's ACK, 9.6's retry, 9.7's backoff, 9.8's priority
 *          queue and 9.10's statistics.
 *
 *          An INSTANCE, not a singleton. Partly because a gateway may one day
 *          want more than one, but mostly because it is what makes the tests
 *          worth anything: two engines on one fake medium exchange a real frame
 *          and its real ACK, instead of each test simulating one side and
 *          asserting what the other side would have done.
 *
 *          Everything is statically allocated (agent rule 6) and nothing here
 *          may be called from interrupt context (rule 5) - boomlink_link_poll()
 *          is a superloop citizen, like radio_process().
 ******************************************************************************
 */
#ifndef BOOMLINK_LINK_H
#define BOOMLINK_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "boomlink_dupcache.h"
#include "boomlink_linkframe.h"
#include "boomlink_port.h"
#include "boomlink_txqueue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Called for each accepted DATA frame's payload, from boomlink_link_poll().
 *
 * The payload is BoomProtocol's business, not this layer's: section 9 is
 * explicit that "BoomLink never decodes the Protobuf payload and has no Nanopb
 * dependency", so this hands over opaque bytes and takes no view of them. A
 * payload that is not a valid Envelope is a BoomProtocol-layer failure counted
 * separately (section 9.2), which is why this callback cannot fail.
 *
 * `payload` points into the engine's RX buffer and is only valid for the
 * duration of the call - copy what you need. A queue of received payloads here
 * would double the RX buffering for no benefit, since the dispatcher runs in the
 * same superloop.
 */
typedef void (*boomlink_link_rx_fn)(void *user, uint32_t source_id, const uint8_t *payload,
                                    size_t payload_len);

typedef struct {
  /* This node's address (section 7.2). Must be a real node ID - 0 and
     0xFFFFFFFF are rejected by boomlink_link_init(), because a node that is
     unconfigured or thinks it is the broadcast address accepts nothing and can
     acknowledge nothing, so bringing the link up in that state would look like a
     working radio with a silent link. */
  uint32_t node_id;
  /* Network ID (section 7.3). BOOMLINK_LINKFRAME_MAGIC_DEFAULT unless two
     deployments share a channel. */
  uint8_t  magic;

  /* Section 9.6: "ACK timeout derived from/configured for the active radio
     profile rather than assuming one fixed timeout for every spreading factor
     and packet size". The engine computes
     airtime(frame) + airtime(ack) + ack_timeout_margin_ms, so the profile-
     dependent part comes from the port and this margin covers what airtime does
     not: the peer's turnaround, superloop latency at both ends, and clock
     granularity. */
  uint32_t ack_timeout_margin_ms;
  /* Section 9.6: "bounded retry count, runtime-configurable; default target: 3
     total transmission attempts". Total attempts, not retries after the first -
     1 means send once and never retry. 0 is rejected. */
  uint8_t  max_attempts;
  /* Section 9.7's randomized retry backoff, drawn uniformly from
     [backoff_min_ms, backoff_max_ms]. Equal values mean a fixed delay, which is
     legal but defeats the purpose: the point is that nodes which collided once
     do not collide again in lockstep. max < min is rejected. */
  uint32_t backoff_min_ms;
  uint32_t backoff_max_ms;

  /* Where accepted payloads go. May be NULL, in which case DATA frames are still
     validated, deduplicated, acknowledged and counted, but their payloads are
     discarded - which is what a monitoring node wants. */
  boomlink_link_rx_fn on_rx;
  void               *on_rx_user;
} boomlink_link_config_t;

/**
 * Section 9.10's link statistics. Every counter that section lists, plus three
 * it does not, each earning its place by covering a drop that would otherwise be
 * invisible:
 *
 *   ack_unmatched     - an ACK arrived addressed to this node that acknowledges
 *                       nothing it is waiting for. Late (the frame already timed
 *                       out and was retried), or forged. Distinct from
 *                       ack_received, which counts the useful ones. An ACK
 *                       addressed to somebody else is NOT counted here - that is
 *                       ordinary overheard traffic and lands in
 *                       rx_other_destination like any other frame not for us.
 *   rx_invalid_source - a frame whose source address cannot be a distinct peer:
 *                       the unconfigured address, the broadcast address, or this
 *                       node's OWN ID. The last is the interesting one - it is a
 *                       reflection, a misconfigured twin, or a spoof, and
 *                       delivering it would have this node acknowledge itself and
 *                       feed its own duplicate cache under its own key.
 *   rx_oversize       - a packet longer than the port declared it can carry
 *                       (max_packet), so only a prefix was staged and the rest is
 *                       unknowable. Not "longer than the radio's ceiling": with a
 *                       reduced radio profile the limit is that profile's, which
 *                       is exactly the case a fixed 255-byte check would miss.
 */
typedef struct {
  uint32_t tx_envelopes;
  uint32_t rx_envelopes;
  uint32_t tx_retries;
  uint32_t tx_failures;
  uint32_t rx_duplicates;
  uint32_t rx_malformed;
  uint32_t rx_other_destination;
  uint32_t rx_rejected_magic_or_version;
  uint32_t ack_sent;
  uint32_t ack_received;
  uint32_t ack_unmatched;
  uint32_t rx_invalid_source;
  uint32_t rx_oversize;
  /* Section 9.10: "cumulative TX airtime (for duty-cycle verification, section
     6.1)". Microseconds, from the port's estimate, accumulated over every
     transmission INCLUDING retries and ACKs - duty cycle is about what the
     radio actually radiated, not about what succeeded. */
  uint64_t tx_airtime_us;
  float    last_rssi_dbm;
  float    last_snr_db;
} boomlink_link_stats_t;

typedef struct {
  boomlink_link_config_t config;
  boomlink_port_t        port;
  boomlink_dupcache_t    dupcache;
  boomlink_txqueue_t     txqueue;
  boomlink_link_stats_t  stats;

  /* Section 9.3: "session_id generated once per boot, sequence incremented for
     each transmitted envelope". */
  uint32_t session_id;
  uint32_t next_sequence;

  /* One RX staging buffer, sized to the largest packet ANY port may declare, so
     that the port's own max_packet is always the binding limit and the engine
     never has to take the smaller of two ceilings. Reused across packets;
     nothing outlives boomlink_link_poll(). */
  uint8_t rx_buffer[BOOMLINK_PORT_MAX_PACKET];
} boomlink_link_t;

/**
 * Bring the engine up. Returns false, leaving `link` unusable, if the config or
 * the port is invalid - see the individual fields for what is rejected and why.
 *
 * `session_id` is a parameter rather than drawn from the port's RNG on purpose.
 * Section 9.3's "a fresh session_id on reboot makes reboot behaviour explicit"
 * only works if it actually differs across boots, and a PRNG seeded the same way
 * every boot - which is exactly what a microcontroller with no entropy at reset
 * gives you - would produce the SAME session forever. The peer would then treat
 * every frame after a reboot as a replay of one it had already seen and go deaf
 * to this node until its own cache evicted the entry. Making it a parameter puts
 * that requirement in front of whoever brings up the firmware instead of hiding
 * it in here.
 *
 * `session_id` 0 is refused, which is the one value that requirement fails at
 * most often: an unseeded PRNG, a zeroed struct, a field nobody filled in. It
 * cannot be detected in general - a caller that passes 7 every boot is equally
 * broken and equally invisible - but 0 is the value that arrives by ACCIDENT,
 * and refusing it turns the most likely instance of that mistake into a
 * bring-up failure. Received frames carrying session 0 are NOT rejected: what a
 * peer chooses for its own session is not this node's business to police, and
 * dropping such traffic would break interoperability over a hygiene rule that
 * the wire format (section 7.3) does not impose.
 */
bool boomlink_link_init(boomlink_link_t *link, const boomlink_link_config_t *config,
                        const boomlink_port_t *port, uint32_t session_id);

/**
 * Queue `payload` for `destination_id`. Does not transmit - boomlink_link_poll()
 * does that, so that sequence assignment stays at dequeue (section 9.1).
 *
 * `request_ack` is honoured for unicast and forced OFF for the broadcast
 * address, per section 9.9's "broadcast never requests link ACK". Forced rather
 * than rejected because a caller broadcasting a detection event should not have
 * to know the rule; the rule exists to prevent an ACK storm, and silently
 * getting it right is better than an error the caller has to handle.
 *
 * @return the queue's verdict, so a caller can tell "queued" from "the queue is
 *         full of more urgent traffic" from "too long to ever send".
 */
boomlink_txqueue_result_t boomlink_link_send(boomlink_link_t *link, uint32_t destination_id,
                                             boomlink_tx_priority_t priority,
                                             bool request_ack, const uint8_t *payload,
                                             size_t payload_len);

/**
 * Service the link: drain every packet the radio has, then transmit if the TX
 * pipeline is free. Call every superloop iteration, never from an ISR (rule 5).
 *
 * Draining RX before servicing TX is deliberate and matches section 9.2's
 * pipeline order. An ACK that has already arrived should be consumed before the
 * TX side considers whether the frame it acknowledges has timed out, or a
 * loaded superloop could produce a retry for a frame that was in fact
 * acknowledged - burning airtime and forcing the peer to suppress a duplicate.
 */
void boomlink_link_poll(boomlink_link_t *link);

/**
 * Copy out the statistics (section 9.10). A NULL argument is ignored rather than
 * dereferenced: on the target these are called from diagnostics and CLI paths,
 * where a hard fault is a worse outcome than a missing reading.
 */
void boomlink_link_get_stats(const boomlink_link_t *link, boomlink_link_stats_t *out);

/**
 * Zero every counter, and NOTHING else. Not a reset of the link: the session,
 * the sequence, the duplicate cache and any frame awaiting an ACK all survive.
 * Zeroing those would be far worse than losing the counters - a fresh sequence
 * inside a live session is a replay of numbers the peer's duplicate window has
 * already seen, and it would go deaf to this node until the entry aged out.
 */
void boomlink_link_reset_stats(boomlink_link_t *link);

/** This node's session ID (section 9.3), for diagnostics. 0 for a NULL link,
 *  which is the "never assigned" value boomlink_link_init() refuses. */
uint32_t boomlink_link_session_id(const boomlink_link_t *link);

#ifdef __cplusplus
}
#endif

#endif /* BOOMLINK_LINK_H */
