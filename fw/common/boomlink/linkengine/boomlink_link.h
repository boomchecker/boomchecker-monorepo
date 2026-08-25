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
 *
 * `destination_id` is the frame's own addressing - either this node's `node_id`
 * (unicast) or BOOMLINK_ADDR_BROADCAST - and it is the ONLY place a caller can
 * learn which one this was. Section 7.1 keeps addressing out of the Protobuf
 * payload entirely ("addressing... live in the fixed binary link frame header
 * ... not in Protobuf"), so nothing above this layer can recover it once the
 * payload has been handed over. This matters concretely for section 9.9's
 * "commands that are dangerous when broadcast should be rejected by the
 * application service unless explicitly designed for broadcast" - a
 * CommandService built on this callback cannot enforce that rule without this
 * field, since a rejected-when-broadcast command looks identical to an
 * accepted unicast one in every other parameter here.
 *
 * `rssi_dbm`/`snr_db` are THIS packet's signal quality, not the "last" value
 * boomlink_link_stats_t carries - the distinction matters exactly when it looks
 * like it wouldn't: boomlink_link_poll() drains every packet the radio is
 * holding in a single call (a burst arriving faster than the superloop polls,
 * which section 9.7's whole reason for jitter is several nodes producing at
 * once), and a caller reading the stats field instead of this parameter after
 * the drain has moved on would attribute the LAST packet's signal quality to
 * every one of them. Section 11's gateway forwarding needs per-detection
 * RSSI/SNR for exactly this reason. The stats field remains for a caller that
 * only wants a spot check, not a per-event record.
 *
 * Calling boomlink_link_send() from here is fine. Calling boomlink_link_poll()
 * is NOT: this pointer is into the one staging buffer the next packet would be
 * read into, so a re-entrant poll would replace the payload underneath a handler
 * that is still reading it.
 *
 * This signature has grown a parameter in two separate review rounds
 * (`destination_id`, then `rssi_dbm`/`snr_db`) and costs nothing to grow again
 * today, since no firmware code implements it yet - only this package's own
 * test doubles do. That stops being true the moment Phase C wires a real
 * dispatcher against it: a third addition then breaks every real
 * implementation instead of two test files. If another field turns out to be
 * needed after that point, collecting these into one `boomlink_rx_info_t`
 * parameter is the change to make then, not something worth doing pre-emptively
 * now against a need that has not materialized.
 */
typedef void (*boomlink_link_rx_fn)(void *user, uint32_t source_id, uint32_t destination_id,
                                    const uint8_t *payload, size_t payload_len,
                                    float rssi_dbm, float snr_db);

/** How a queued frame's time in the TX pipeline ended. */
typedef enum {
  /* Transmitted, and no ACK was requested - so this is as much as the link can
     ever know. Not "delivered": an unacknowledged frame that reached the air may
     or may not have been heard, which is precisely what ack_requested buys. */
  BOOMLINK_TX_SENT = 0,
  /* The peer acknowledged it. The only outcome that means delivery. */
  BOOMLINK_TX_ACKED,
  /* Section 9.6's "final failure": every permitted attempt was transmitted and
     none was acknowledged. */
  BOOMLINK_TX_NO_ACK,
} boomlink_tx_outcome_t;

/**
 * Called when the TX pipeline is finished with a frame - section 9.6's "final
 * failure is surfaced to the caller and counted in link statistics". Optional.
 *
 * Reported for success as well as failure, because a caller that only hears
 * about failures cannot tell "delivered" from "still queued behind something",
 * and at this traffic rate a detection event waiting on a retry sequence can sit
 * in the pipeline for seconds.
 *
 * NOT one call per accepted boomlink_link_send(), and a caller that pairs them
 * one-to-one will leak a tracking entry every time this happens: a frame
 * accepted with BOOMLINK_LINK_SEND_OK can afterwards be EVICTED from the queue
 * by more urgent traffic (section 9.8). Such a frame never enters the pipeline,
 * so it never receives a sequence - which is also why it cannot be reported
 * here, since `sequence` is the only handle this callback gives a caller for
 * saying which frame it means. Its trace is the tx_shed counter and the
 * BOOMLINK_LINK_SEND_OK_EVICTED handed to whichever send displaced it.
 * Reporting evictions properly would mean giving a queued frame an identity
 * before dequeue, which section 9.1 deliberately does not do; if a consumer
 * needs it, that is the design question to reopen rather than a counter to add.
 *
 * `sequence` is the on-air sequence every attempt carried (section 9.6 reuses it
 * on retransmission), so a caller can correlate this with what a peer reports
 * seeing. `attempts` is how many transmissions the radio actually accepted.
 *
 * `sequence` is NOT known until this callback fires, so it cannot serve as an
 * enqueue-time handle: a caller queueing two ACK-requested frames to the same
 * peer close together cannot tell which completion belongs to which
 * boomlink_link_send() call, since priority reordering can transmit (and
 * report) the second one first. Section 9.6 already names the fix for this at
 * the application layer rather than here: "application request/response
 * correlation uses request_id, not sequence number" - a caller that needs to
 * tell two of its own in-flight sends apart puts its own identifier in the
 * payload, the same as it would to correlate a CommandResponse.
 *
 * `rssi_dbm`/`snr_db` are the ACKing frame's OWN signal quality for
 * BOOMLINK_TX_ACKED - the same per-event reasoning boomlink_link_rx_fn's
 * identically-named parameters exist for, and not a coincidence: an ACK is
 * still a received packet, and reading link->stats.last_rssi_dbm instead would
 * go stale the moment a burst drains more than one packet in the same
 * boomlink_link_poll() call and the ACK isn't the last of them. 0.0f for
 * BOOMLINK_TX_SENT and BOOMLINK_TX_NO_ACK, where nothing was received to
 * report a reading for - this codebase's existing "nothing to report"
 * sentinel for signal quality, not a plausible-looking measurement.
 *
 * Called from boomlink_link_poll() with the pipeline ALREADY back to idle, so
 * calling boomlink_link_send() from here is safe and the frame it queues is
 * simply the next one considered. Re-entering boomlink_link_poll() is not - see
 * boomlink_link_rx_fn.
 */
typedef void (*boomlink_link_tx_done_fn)(void *user, boomlink_tx_outcome_t outcome,
                                        uint32_t destination_id, uint32_t sequence,
                                        uint8_t attempts, float rssi_dbm, float snr_db);

/** What boomlink_link_send() did with a frame. */
typedef enum {
  /* Queued. */
  BOOMLINK_LINK_SEND_OK = 0,
  /* Queued, and strictly lower-priority traffic was shed to make room (section
     9.8's drop policy working as intended, not a fault). */
  BOOMLINK_LINK_SEND_OK_EVICTED,
  /* Not queued: the queue is full of traffic at least as urgent. */
  BOOMLINK_LINK_SEND_QUEUE_FULL,
  /* Not queued: header + payload exceeds what this port's radio can carry
     (section 7.3's "an oversized frame is rejected before transmission"), so it
     could never have been sent however long it waited. */
  BOOMLINK_LINK_SEND_TOO_LONG,
  /* Not queued: section 9.1's "enforce destination rules" - the unconfigured
     address, or this node itself. */
  BOOMLINK_LINK_SEND_BAD_DESTINATION,
} boomlink_link_send_result_t;

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

  /* Section 9.7's OTHER requirement, and the one that section actually opens
     with: "the link layer therefore supports configurable randomized TX jitter
     for event messages", with retry backoff named as something that must ALSO
     have jitter. Drawn uniformly from [0, tx_jitter_max_ms] and applied before a
     frame's FIRST transmission; retransmissions use the backoff above instead.
     0 disables it.

     This is the half that addresses the problem 9.7 describes. Backoff only
     helps nodes that have already collided once; the collision itself happens
     because several nodes detect the same gunshot within milliseconds and every
     one of them transmits immediately. Without this they collide on the first
     attempt every time, and the retry that follows is the only thing separating
     them.

     Applied to every queued frame rather than only to "event messages", because
     the engine cannot tell one: section 9 forbids it from decoding the payload,
     so a detection event and a telemetry reading are the same opaque bytes here.
     That is not a compromise at this traffic rate - the cost is a few tens of
     milliseconds of latency on traffic that is not latency-critical, and section
     9.7 explicitly accepts it ("the original detection timestamp is captured
     before this delay, so localization timing is not changed by radio
     scheduling"). ACKs are unaffected: they never pass through the queue.

     A caller that needs one class of traffic sent without jitter should raise
     its priority - which does not skip the delay, but does mean it is the frame
     the delay is spent on. If that proves insufficient, per-priority jitter
     belongs here and nowhere else. */
  uint32_t tx_jitter_max_ms;

  /* Where accepted payloads go. May be NULL, in which case DATA frames are still
     validated, deduplicated, acknowledged and counted, but their payloads are
     discarded - which is what a monitoring node wants. */
  boomlink_link_rx_fn on_rx;
  void               *on_rx_user;

  /* Where a finished transmission is reported (section 9.6). May be NULL, in
     which case outcomes are only visible in the statistics. */
  boomlink_link_tx_done_fn on_tx_done;
  void                    *on_tx_done_user;
} boomlink_link_config_t;

/**
 * Section 9.10's link statistics. Every counter that section lists, plus five
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
 *   tx_dropped        - a frame boomlink_link_send() refused, so it never entered
 *                       the queue at all: a full queue, an oversize payload, or a
 *                       destination section 9.1 forbids. The caller learns WHICH
 *                       from the return value; this counter exists because a link
 *                       shedding traffic before it ever reaches the radio would
 *                       otherwise be invisible in the statistics - every counter
 *                       section 9.10 lists describes what happened AFTER queueing.
 *   tx_shed           - lower-priority traffic evicted to make room for something
 *                       more urgent (section 9.8's policy). Counted apart from
 *                       tx_dropped on purpose: this is the queue working as
 *                       designed, and a node shedding telemetry to protect
 *                       detections should not read as a node in trouble.
 *
 * On the two TX counters section 9.10 does list: tx_envelopes counts each
 * envelope's FIRST transmission and tx_retries counts retransmissions of one, so
 * transmissions on the air is their sum and neither double-counts the other.
 * tx_failures counts a frame that exhausted section 9.6's attempts unacknowledged
 * PLUS every transmission the radio refused - the second is not a lost frame (it
 * stays queued and is retried) but it is airtime that did not happen, and a radio
 * refusing constantly is the thing an operator needs to see.
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
  uint32_t tx_dropped;
  uint32_t tx_shed;
  /* Section 9.10: "cumulative TX airtime (for duty-cycle verification, section
     6.1)". Microseconds, from the port's estimate, accumulated over every
     transmission INCLUDING retries and ACKs - duty cycle is about what the
     radio actually radiated, not about what succeeded. */
  uint64_t tx_airtime_us;
  /* Section 9.10's "last RSSI"/"last SNR" - the signal quality of the last
     packet the RADIO DELIVERED, whatever it turned out to be, updated before
     magic/version/length are checked. Deliberate: an operator watching this
     field wants to know the channel is noisy or a foreign transmitter is in
     range, and a burst of foreign or malformed traffic IS that diagnostic
     signal - discarding it here would make the one field meant to answer
     "is the channel healthy" blind to exactly the traffic that answers it.
     For per-EVENT signal quality of an accepted frame - which is what a
     drained burst needs, since this field only ever holds the last one -
     use the rssi_dbm/snr_db parameters boomlink_link_rx_fn receives instead. */
  float    last_rssi_dbm;
  float    last_snr_db;
} boomlink_link_stats_t;

/**
 * Where the one frame the TX pipeline is working on stands. Section 9.6:
 * "BoomLink v1 is stop-and-wait: at most one ACK-pending frame is outstanding at
 * any time, globally. While waiting for an ACK the TX queue is held."
 */
typedef enum {
  /* Nothing in the pipeline. The next poll dequeues, if anything is queued. */
  BOOMLINK_TX_STATE_IDLE = 0,
  /* A frame is held, with its sequence already assigned, and the next poll
     transmits it. Reached by a jitter or backoff delay expiring - and,
     importantly, by a transmission the radio REFUSED: the frame stays here with
     its sequence rather than being lost. */
  BOOMLINK_TX_STATE_READY,
  /* Section 9.7's pre-transmission jitter, before the FIRST attempt. A distinct
     state rather than a reuse of BACKOFF below: the two delays have different
     causes and different durations, and a diagnostic (or a test) that could not
     tell them apart could not tell "we are spreading out a simultaneous
     detection" from "we already collided and are retrying". */
  BOOMLINK_TX_STATE_JITTER,
  /* Transmitted; waiting for the matching ACK or for the timeout. Nothing else is
     dequeued in this state, which is what makes it stop-and-wait. */
  BOOMLINK_TX_STATE_WAIT_ACK,
  /* The ACK timed out, attempts remain, and section 9.7's randomized delay is
     running before the retransmission. */
  BOOMLINK_TX_STATE_BACKOFF,
} boomlink_tx_state_t;

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

  /* Section 9.6's stop-and-wait pipeline: exactly one frame, held here from the
     moment it leaves the queue until it is acknowledged or gives up. Holding it
     - rather than transmitting straight out of the queue - is what makes a
     retransmission possible at all, and it is also what stops a busy radio from
     destroying the frame: a refused send leaves this slot untouched. */
  struct {
    boomlink_tx_state_t     state;
    boomlink_txqueue_item_t item;
    /* Built once, when the frame is dequeued, and reused byte for byte on every
       retransmission - section 9.6's "retransmission uses the SAME (session_id,
       sequence) so the receiver can suppress duplicate delivery". Rebuilding it
       per attempt would work only as long as nobody touched the sequence
       assignment, which is exactly the kind of coupling that breaks quietly. */
    boomlink_linkframe_header_t header;
    /* Transmissions the radio ACCEPTED, which is what section 9.6's attempt
       budget counts. A refused send radiated nothing and does not consume one. */
    uint8_t                 attempts;
    /* When the last accepted transmission happened, and the ACK window derived
       from that attempt's own frame length (section 9.6 wants the timeout derived
       from the active profile, not fixed). */
    uint32_t                sent_at_ms;
    uint32_t                ack_window_ms;
    /* Whichever of section 9.7's two randomized delays is running - the jitter
       before the first attempt, or the backoff before a retry. Named for what
       they hold rather than for one of the two users: a field called
       `backoff_ms` carrying a jitter draw is exactly the sort of small untruth
       that makes a state machine hard to read. Which delay it is, is the state. */
    uint32_t                delay_started_ms;
    uint32_t                delay_ms;
  } tx;

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
 * Apply a new retry policy to a LIVE link - section 8.2's `LinkConfig`, a
 * runtime-updatable message group distinct from `GeneralConfig` (which carries
 * `node_id`, not covered here) - without disturbing anything else: the
 * session, the sequence counter, the duplicate cache, the TX queue, and any
 * frame currently in the pipeline all survive untouched.
 *
 * This exists because the only alternative is calling boomlink_link_init()
 * again, and that is the wrong tool for a configuration update: it
 * `memset()`s the whole engine, silently dropping every queued frame with no
 * counter and no on_tx_done callback, and forces a session_id choice that is
 * wrong either way - the same session replays low sequence numbers into a
 * peer's still-live duplicate window (section 9.3's whole reason session_id
 * exists), and a fresh one makes an ordinary settings change look like a
 * reboot to every peer watching for one. Writing the five fields directly
 * bypasses boomlink_link_init()'s validation instead, which is the failure
 * mode boomlink_link_reset_stats() already avoids for statistics and this
 * function avoids for retry policy - by existing as a narrow, validated
 * mutator rather than leaving a caller to choose between two wrong options.
 *
 * `node_id`, `magic`, and the RX/TX-done callbacks are NOT reconfigurable here
 * on purpose, for three DIFFERENT reasons rather than one shared one:
 *
 *   `node_id`  - section 7.2 makes a node's own address something bring-up
 *              decides once, not something a live link should discover it now
 *              has a different opinion about.
 *   `magic`    - section 8.2 never actually assigns this field to a config
 *              group (it is absent from every GeneralConfig/LinkConfig example
 *              the spec gives), but section 7.3 does call it
 *              "runtime-configurable", so the omission looks like an oversight
 *              rather than a decision. What DOES apply, by the same reasoning
 *              section 8.2 gives for RadioConfig, is the hazard: changing this
 *              node's own magic live means every peer's magic check now
 *              rejects it and its own now rejects every peer - "a working
 *              radio with a silent link" is exactly the failure mode section
 *              8.2's revert-on-timeout ceremony exists to prevent for radio
 *              profile changes, and nothing currently extends that ceremony to
 *              this field. Excluded here for the same reason a radio profile
 *              change gets special apply semantics, not because section 7.2's
 *              node-identity argument extends to it - it does not.
 *   callbacks  - a superloop wiring concern with no config-message analogue in
 *              section 8.2 at all.
 *
 * A caller needing to change any of the three genuinely needs a fresh
 * boomlink_link_init() and everything that implies.
 *
 * Validated exactly as boomlink_link_init() validates the same fields - see
 * retry_policy_is_valid() in the .c file, the one place both functions share
 * this check from, so they cannot drift into disagreeing about what a valid
 * policy is.
 *
 * @return false, leaving the link's current policy untouched, if `link` is
 *         NULL or the new policy fails validation (section 9.6's "do not
 *         retry forever" - max_attempts 0 - or an inverted backoff range).
 *
 * Safe to call from boomlink_link_rx_fn or boomlink_link_tx_done_fn, unlike
 * boomlink_link_poll() - this touches only five scalar fields of `link->config`
 * and nothing that either callback's own call chain is still using when it
 * runs, so there is no re-entrancy hazard to avoid.
 *
 * Five scalar parameters rather than a `boomlink_link_retry_policy_t` struct,
 * which was considered: a struct would remove the same-typed-adjacent-argument
 * risk four `uint32_t` parameters in a row carries (a transposed pair compiles
 * silently - see the tests, which pin exactly this with distinct min/max and a
 * nonzero jitter so a transposition cannot hide behind three parameters that
 * all happened to share one value). It was not taken because it would either
 * duplicate `boomlink_link_config_t`'s five fields in a second type this
 * function alone uses, or restructure that struct to nest them - and every
 * existing config literal in this package (see the tests) initializes those
 * five fields flat, by name, so restructuring reaches every call site for a
 * safety property the field NAMES here already provide once a caller reads the
 * signature against boomlink_link_config_t's matching field list. If a sixth
 * policy field is ever added, that trade is worth revisiting.
 */
bool boomlink_link_reconfigure(boomlink_link_t *link, uint32_t ack_timeout_margin_ms,
                               uint8_t max_attempts, uint32_t backoff_min_ms,
                               uint32_t backoff_max_ms, uint32_t tx_jitter_max_ms);

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
 * Two things ARE rejected rather than fixed up, both because there is no
 * sensible correction and silently picking one would be worse:
 *
 *   - Section 9.1's "enforce destination rules". The unconfigured address is
 *     nobody, and this node's own ID would have it talk to itself - a frame the
 *     receiver would drop as an impossible source anyway (see rx_invalid_source),
 *     after paying its airtime. Broadcast is of course allowed.
 *   - A payload that does not fit this port's radio. Checked against the PORT,
 *     not only against the queue's compile-time slot size, because the two differ
 *     on any reduced radio profile - and accepting a frame there that can never be
 *     transmitted means the caller is told "queued" about a frame that will be
 *     destroyed later, which is the report it most needs not to get.
 *
 * @return what happened. Worth checking: the difference between "queued",
 *         "the queue is full of more urgent traffic" and "this can never be sent"
 *         is the difference between waiting, backing off and fixing the caller.
 *
 * `link` must be a brought-up engine; unlike the three diagnostics below, this is
 * not NULL-tolerant. The distinction is deliberate rather than inconsistency:
 * this and boomlink_link_poll() are called from the superloop that owns the
 * engine, where a NULL is a wiring error to find at bring-up, while the
 * diagnostics are reached from a CLI where a missing reading beats a hard fault.
 */
boomlink_link_send_result_t boomlink_link_send(boomlink_link_t *link,
                                               uint32_t destination_id,
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
 *
 * "Drain every packet the radio has" only helps a burst if the PORT can hold
 * more than one between two calls to this function - and the only real port
 * that exists today, fw/bom-stm32node/App/radio/radio.h, cannot: it is
 * single-slot, and its own header says so ("a future consumer... must replace
 * this single-slot model with its own queue"). This engine does not add that
 * queue; it only drains whatever the port is holding when called, which for
 * radio.h is at most one packet regardless of how this loop is written. A
 * burst of near-simultaneous transmissions - the exact "several nodes detect
 * one gunshot" scenario section 9.7's jitter exists to spread out, not
 * eliminate - can still lose all-but-the-latest packet to radio.h's overwrite,
 * invisibly to every BoomLink statistic, on real hardware today. The fake port
 * used by this package's tests buffers the whole burst in its shared "air"
 * log, which is exactly why this gap is invisible from the host side. Giving
 * the port a real queue is Phase C's work, not this one's.
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

/**
 * Where the stop-and-wait pipeline stands (section 9.6). For diagnostics and for
 * tests: without it a test can only infer "the frame is still pending" from the
 * absence of a transmission, which is also what a pipeline that silently dropped
 * the frame looks like. BOOMLINK_TX_STATE_IDLE for a NULL link.
 */
boomlink_tx_state_t boomlink_link_tx_state(const boomlink_link_t *link);

/** Frames waiting in the queue, NOT counting one already in the pipeline. */
size_t boomlink_link_queue_depth(const boomlink_link_t *link);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* BOOMLINK_LINK_H */
