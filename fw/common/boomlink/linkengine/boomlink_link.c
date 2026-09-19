/**
 ******************************************************************************
 * @file    boomlink_link.c
 ******************************************************************************
 */
#include "boomlink_link.h"

#include <string.h>

/* The RX buffer must be able to hold the largest packet any port may declare, so
   that port.max_packet - which boomlink_port_is_valid() bounds by this same
   constant - is always the binding limit. That is what lets the RX path compare
   an incoming length against the PORT's ceiling and not against a second,
   independently chosen buffer size: with two ceilings the code has to take the
   smaller of them, and the version that forgets to is the one that stages a
   200-byte packet on a radio profile that tops out at 64. */
_Static_assert(sizeof(((boomlink_link_t *)0)->rx_buffer) >= BOOMLINK_PORT_MAX_PACKET,
               "the RX buffer must cover the largest packet any port may declare");
_Static_assert(BOOMLINK_PORT_MAX_PACKET >= BOOMLINK_LINKFRAME_HEADER_SIZE,
               "no port could carry even a bare header");

/* Section 7.2's address space, as in the frame layer. Duplicated rather than
   exported from there because it is two comparisons, and exporting it would put
   a predicate on the frame layer's public surface that only this file wants. */
static bool is_valid_node_id(uint32_t node_id) {
  return node_id != BOOMLINK_ADDR_INVALID && node_id != BOOMLINK_ADDR_BROADCAST;
}

/* Forward-declared because the RX path completes a transmission (an arriving ACK
   is what usually ends one) while the TX pipeline is defined below it. The file is
   ordered RX-then-TX to match section 9.2's pipeline order, and this one edge
   between them is the reason the two halves are not fully separable. */
static void finish_tx(boomlink_link_t *link, boomlink_tx_outcome_t outcome, float rssi_dbm,
                      float snr_db);

/**
 * Whether a retry policy (the five knobs section 8.2's LinkConfig carries -
 * everything boomlink_link_config_t has that ISN'T identity, callbacks, or
 * state) is one the engine can run with. Shared between boomlink_link_init()
 * and boomlink_link_reconfigure() so the two can never validate this subset
 * differently - a reconfigure that accepted what init would have refused would
 * make "was this link ever validly configured" depend on which function last
 * touched it.
 */
static bool retry_policy_is_valid(uint8_t max_attempts, uint32_t backoff_min_ms,
                                  uint32_t backoff_max_ms) {
  /* Section 9.6 says "do not retry forever"; zero attempts is the opposite
     mistake and would mean queued frames are never sent at all. */
  if (max_attempts == 0u) {
    return false;
  }
  if (backoff_max_ms < backoff_min_ms) {
    return false;
  }
  return true;
}

bool boomlink_link_init(boomlink_link_t *link, const boomlink_link_config_t *config,
                        const boomlink_port_t *port, uint32_t session_id) {
  if (link == NULL || config == NULL || !boomlink_port_is_valid(port)) {
    return false;
  }
  /* A node with no valid identity accepts nothing (section 7.2) and can
     acknowledge nothing, so a link brought up in that state is a radio that
     transmits into a void - which looks like working hardware. Refusing here
     turns a silent misconfiguration into a bring-up failure. */
  if (!is_valid_node_id(config->node_id)) {
    return false;
  }
  if (!retry_policy_is_valid(config->max_attempts, config->backoff_min_ms,
                             config->backoff_max_ms)) {
    return false;
  }
  /* Section 9.3's "fresh session_id on reboot" only means anything if one was
     actually supplied - see the header for why 0 specifically is refused and why
     received frames carrying it are not. */
  if (session_id == 0u) {
    return false;
  }

  memset(link, 0, sizeof(*link));
  link->config     = *config;
  link->port       = *port;
  link->session_id = session_id;
  /* Sequence starts at 1, not 0. Nothing in section 9.3 requires it, but it
     keeps "sequence 0" available as an unmistakable "never assigned" value in a
     log or a debugger, and costs one number out of 2^32. */
  link->next_sequence = 1u;
  boomlink_dupcache_init(&link->dupcache);
  boomlink_txqueue_init(&link->txqueue);
  return true;
}

bool boomlink_link_reconfigure(boomlink_link_t *link, uint32_t ack_timeout_margin_ms,
                               uint8_t max_attempts, uint32_t backoff_min_ms,
                               uint32_t backoff_max_ms, uint32_t tx_jitter_max_ms) {
  if (link == NULL || !retry_policy_is_valid(max_attempts, backoff_min_ms, backoff_max_ms)) {
    return false;
  }
  link->config.ack_timeout_margin_ms = ack_timeout_margin_ms;
  link->config.max_attempts          = max_attempts;
  link->config.backoff_min_ms        = backoff_min_ms;
  link->config.backoff_max_ms        = backoff_max_ms;
  link->config.tx_jitter_max_ms      = tx_jitter_max_ms;
  return true;
}

/**
 * Transmit one already-encoded frame, accumulating airtime and failures.
 *
 * Does NOT re-check `len` against the port's ceiling, and the absence is
 * deliberate rather than an omission. Section 7.3's "an oversized frame is
 * rejected before transmission" is enforced at the only place a caller can be
 * told about it - boomlink_link_send(), which refuses the payload outright - and
 * both routes here are bounded by construction: a DATA frame's length is fixed
 * when it is queued, and an ACK is exactly a header, which boomlink_port_is_valid()
 * guarantees fits. A second check here would be a branch no test could reach, and
 * an unreachable branch that looks like a safety net is worse than none: it reads
 * as covered ground. If the invariant were ever broken the driver's own send()
 * would refuse the frame, which lands in the ordinary refusal path below.
 */
static bool transmit(boomlink_link_t *link, const uint8_t *frame, size_t len) {
  const int result = link->port.send(link->port.ctx, frame, len);
  if (result != 0) {
    link->stats.tx_failures++;
    return false;
  }
  /* Counted for every transmission that the radio accepted, retries and ACKs
     included: section 6.1's duty cycle is about what was radiated, not about
     what was useful. */
  link->stats.tx_airtime_us += link->port.airtime_us(link->port.ctx, len);
  return true;
}

/**
 * Send an ACK for `received`, per section 9.5.
 *
 * Sent immediately rather than queued. An ACK that waits behind queued traffic
 * can easily miss the sender's ACK timeout and provoke the very retry it exists
 * to prevent - and since it is 20 bytes with no payload, queueing buys nothing.
 *
 * Sent immediately EVEN WHILE this node is itself in BOOMLINK_TX_STATE_WAIT_ACK,
 * which is worth stating because service_tx() says the opposite about its own
 * traffic ("the queue is held... transmitting anything now would mean not hearing
 * the ACK"). Both are deliberate; the asymmetry is not an oversight, and an
 * earlier version of these two comments did read as a contradiction.
 *
 * The radio is half-duplex, so this ACK genuinely can mask an incoming ACK for
 * our own pending frame, costing one retry. Deferring it is worse, not safer:
 * the peer waiting on this ACK has an ACK window of the same order as ours, so a
 * deferral until our wait ends makes ITS retry near-certain, where transmitting
 * now only loses ours if the two transmissions actually overlap - one ACK's
 * airtime inside a window several times longer. So the choice is between a
 * certain retry on the peer's side and an occasional one on ours.
 *
 * What is held during our ACK wait is the QUEUE, which is the case section 9.6
 * is actually about: a queued DATA frame can be seconds of airtime at a high
 * spreading factor, and it has somewhere to wait. An ACK has neither property.
 * If measurements ever show this trade going the wrong way, the fix is CAD or
 * time slots (section 9.7's "later MAC improvements"), not a delayed ACK.
 */
static void send_ack(boomlink_link_t *link, const boomlink_linkframe_header_t *received) {
  boomlink_linkframe_header_t ack;
  if (!boomlink_linkframe_make_ack(received, link->config.node_id, &ack)) {
    /* Unreachable, and deliberately not counted. make_ack() refuses only when an
       endpoint address is not a real node ID (section 7.2), and both were already
       checked: the source by source_is_usable() below, this node's own ID by
       boomlink_link_init(). The branch stays because make_ack() is
       warn_unused_result and ignoring its answer would put 20 bytes of zeroed
       header on the air, under section 6.1's duty-cycle budget, for no receiver.
       An earlier version counted it in a statistic of its own; that counter was
       removed because no test could ever move it, and a counter nobody can reach
       reads as covered ground when it is the opposite. If this ever fires, the
       source check and this call have drifted apart. */
    return;
  }
  uint8_t bytes[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&ack, bytes);
  if (transmit(link, bytes, sizeof(bytes))) {
    link->stats.ack_sent++;
  }
}

/**
 * Whether a frame claiming to come from `source_id` can be acted on at all.
 *
 * Three rejections, and the third is the one worth spelling out. The
 * unconfigured and broadcast addresses are not something a node can BE (section
 * 7.2), so a frame from either is not from a peer. Our OWN node ID is the same
 * problem arrived at differently: a reflection off a repeater, a second board
 * flashed with the same ID, or a spoof. Accepting it would have this node
 * acknowledge itself, and file the frame in its duplicate cache under its own
 * key - so its own later traffic could be suppressed as a duplicate of it.
 */
static bool source_is_usable(const boomlink_link_t *link, uint32_t source_id) {
  return is_valid_node_id(source_id) && source_id != link->config.node_id;
}

/** One received packet, through section 9.2's pipeline. */
static void handle_packet(boomlink_link_t *link, size_t len, float rssi, float snr) {
  link->stats.last_rssi_dbm = rssi;
  link->stats.last_snr_db   = snr;

  if (len > link->port.max_packet) {
    /* Longer than the PORT said it can carry, so only a prefix was staged and the
       rest is unknowable. Compared against max_packet rather than against the RX
       buffer's own size on purpose: those are the same number only for a radio at
       full capacity, and on a reduced profile - a smaller max_packet - a
       buffer-sized check would happily stage and parse a packet the radio could
       not have produced. Counted separately from malformed so a flood of these is
       recognisable as a foreign transmitter or a driver fault rather than as
       corrupt traffic. */
    link->stats.rx_oversize++;
    return;
  }

  boomlink_linkframe_header_t header;
  size_t                      payload_len = 0u;
  const boomlink_linkframe_parse_result_t result = boomlink_linkframe_parse(
      link->rx_buffer, len, link->config.magic, &header, &payload_len);

  switch (result) {
    case BOOMLINK_LINKFRAME_OK:
      break;
    case BOOMLINK_LINKFRAME_ERR_MAGIC:
    case BOOMLINK_LINKFRAME_ERR_VERSION:
      /* Section 7.3 requires these dropped and counted "before any further
         processing", and section 9.2 counts them apart from malformed traffic:
         another deployment sharing the channel is not a fault, and conflating
         the two would make a neighbouring network look like corruption. */
      link->stats.rx_rejected_magic_or_version++;
      return;
    case BOOMLINK_LINKFRAME_ERR_TOO_SHORT:
    case BOOMLINK_LINKFRAME_ERR_FRAME_TYPE:
    case BOOMLINK_LINKFRAME_ERR_FRAGMENTED:
    case BOOMLINK_LINKFRAME_ERR_ACK_HAS_PAYLOAD:
    case BOOMLINK_LINKFRAME_RESULT_COUNT:
    default:
      link->stats.rx_malformed++;
      return;
  }

  if (header.frame_type == BOOMLINK_FRAME_TYPE_ACK) {
    /* Section 9.2 lists ACK matching before destination validation, and the
       matcher itself requires the ACK to be addressed to this node - but an ACK
       for SOMEBODY ELSE still has to be classified, and "unmatched" is the wrong
       answer for it. ack_unmatched means "an ACK meant for me that acknowledges
       nothing I am waiting for", which is a late or forged ACK and worth looking
       at; an ACK between two other nodes is ordinary overheard traffic on a
       shared medium, arrives constantly, and belongs in rx_other_destination
       with every other frame not for us. Counting it as unmatched would bury the
       first in the second.
       Note the comparison is exact equality, not is_for_node(): a
       broadcast-addressed ACK is not "for everyone", it is malformed by section
       9.5's own matching rule, which requires an ACK's destination to equal the
       receiving node's ID. */
    if (header.destination_id != link->config.node_id) {
      link->stats.rx_other_destination++;
      return;
    }
    /* Section 9.2's "match ACK frames against the pending TX". The matcher lives
       in the frame layer, pinned there against near-miss vectors, because the
       error that hides is an OVER-PERMISSIVE match - one comparing only
       (session_id, sequence) accepts another node's ACK for its own traffic, and
       every delivery test still passes because a correct ACK matches too. */
    if (link->tx.state == BOOMLINK_TX_STATE_WAIT_ACK &&
        boomlink_linkframe_ack_matches(&link->tx.header, &header, link->config.node_id)) {
      link->stats.ack_received++;
      /* This ACK's own rssi/snr, not link->stats.last_rssi_dbm - see finish_tx()'s
         comment for why reading the field back instead would be a stale value
         waiting to happen. */
      finish_tx(link, BOOMLINK_TX_ACKED, rssi, snr);
      return;
    }
    /* Addressed to us but acknowledging nothing we are waiting for: late (the
       frame already timed out and was retried, and this is the ACK of an earlier
       attempt), or forged. */
    link->stats.ack_unmatched++;
    return;
  }

  /* Section 7.2's acceptance rule, via the frame layer so the engine cannot
     drift from it. */
  if (!boomlink_linkframe_is_for_node(header.destination_id, link->config.node_id)) {
    link->stats.rx_other_destination++;
    return;
  }

  /* Checked AFTER the destination, so that a frame not for us is counted as
     somebody else's rather than as an addressing fault - most traffic on a shared
     medium is not for us, and it is not this node's job to audit it. */
  if (!source_is_usable(link, header.source_id)) {
    link->stats.rx_invalid_source++;
    return;
  }

  /* Section 9.4. Note the duplicate check happens BEFORE the ACK is generated,
     and the ACK is still sent for a duplicate: "if it is unicast and requests
     ACK, transmit the ACK again". A duplicate arrives precisely because the
     sender did not hear the first ACK, so staying silent would guarantee it
     retries again until it gives up. */
  const bool duplicate =
      boomlink_dupcache_check(&link->dupcache, header.source_id, header.session_id,
                              header.sequence) == BOOMLINK_DUPCACHE_DUPLICATE;

  /* An ACK is owed for any unicast frame that asked for one, duplicate or not.
     Never for a broadcast frame, whatever its flags say: that is the ACK-storm
     vector recorded in section 9.5 as a receiver-side responsibility the frame
     layer cannot take on, since make_ack() builds such an ACK quite happily. */
  if (header.ack_requested && header.destination_id != BOOMLINK_ADDR_BROADCAST) {
    send_ack(link, &header);
  }

  if (duplicate) {
    link->stats.rx_duplicates++;
    return;
  }

  link->stats.rx_envelopes++;
  if (link->config.on_rx != NULL) {
    /* rssi/snr here MUST be these local parameters, not link->stats.last_rssi_dbm
       /last_snr_db - even though they are bit-identical at this exact point,
       since nothing between the assignment at the top of this function and here
       can have changed either one. That equality is an artefact of this
       function's own single-threaded, non-reentrant shape, not a guarantee: no
       test can tell the two apart today, and none should be expected to - the
       property that matters is stated and tested elsewhere (see
       boomlink_link_rx_fn's doc comment and
       test_a_drained_burst_reports_each_packets_own_signal_quality), which is
       that a multi-packet drain must not converge every call to the LAST
       packet's reading. Reading from stats here would keep passing that test
       for the wrong reason, until the day this function's shape changes enough
       to make the two diverge - silently, since nothing pins which one this
       line is supposed to read from except this comment. */
    link->config.on_rx(link->config.on_rx_user, header.source_id, header.destination_id,
                       &link->rx_buffer[BOOMLINK_LINKFRAME_HEADER_SIZE], payload_len,
                       rssi, snr);
  }
}

/**
 * How long to wait for the ACK of a `frame_len`-byte transmission, in
 * milliseconds.
 *
 * Section 9.6 requires this "derived from/configured for the active radio profile
 * rather than assuming one fixed timeout for every spreading factor and packet
 * size", which is why the airtime comes from the port: at SF12 one full packet is
 * seconds of airtime, and a timeout that ignored that would expire before the
 * frame had even finished going out. Three terms:
 *
 *   the frame's own airtime  - send() returns when the radio ACCEPTED the packet,
 *                              not when it finished radiating it, so all of this
 *                              is still ahead of us when the clock is read;
 *   an ACK's airtime         - a bare header, the smallest packet there is;
 *   ack_timeout_margin_ms    - everything airtime cannot know: the peer's
 *                              turnaround, superloop latency at both ends, and
 *                              millisecond clock granularity.
 *
 * Rounded UP, and that matters more than it looks: a timeout expiring one
 * millisecond into the ACK provokes exactly the retransmission the ACK existed to
 * prevent, and then the peer suppresses the duplicate and re-ACKs, so the link
 * pays double airtime to arrive where it already was.
 */
static uint32_t ack_window_ms(const boomlink_link_t *link, size_t frame_len) {
  const uint64_t frame_us = link->port.airtime_us(link->port.ctx, frame_len);
  const uint64_t ack_us =
      link->port.airtime_us(link->port.ctx, BOOMLINK_LINKFRAME_HEADER_SIZE);
  const uint64_t total = ((frame_us + ack_us + 999u) / 1000u) +
                         (uint64_t)link->config.ack_timeout_margin_ms;
  /* 64-bit throughout and clamped, because a port is free to report large airtime
     values and the margin is a full uint32_t - the sum overflows a uint32_t long
     before either term is implausible, and an overflowed window is a frame that
     times out immediately and retries as fast as the superloop runs. */
  return total > (uint64_t)UINT32_MAX ? UINT32_MAX : (uint32_t)total;
}

/**
 * Section 9.7's randomized retry delay, uniform over
 * [backoff_min_ms, backoff_max_ms].
 *
 * The randomization is the whole point: several nodes detecting the same gunshot
 * transmit at almost the same moment, and a FIXED retry delay has them collide
 * again at the same offset, indefinitely. Equal bounds are legal and mean exactly
 * that, which is why the configuration allows it but the default should not.
 *
 * The modulo introduces a bias towards the low end of the range when the span
 * does not divide 2^32. Named rather than corrected: this is collision avoidance
 * between a handful of nodes, not sampling, and the bias is under one part in
 * 10^7 for any span this configuration would use. Section 14 keeps cryptographic
 * quality a separate concern.
 */
static uint32_t draw_in_range_ms(boomlink_link_t *link, uint32_t lo, uint32_t hi) {
  if (hi <= lo) {
    return lo;
  }
  const uint32_t span = hi - lo;
  if (span == UINT32_MAX) {
    /* span + 1 would be 0, and a modulo by zero is undefined - the whole range is
       already what the generator produces, so use it directly. */
    return link->port.random_u32(link->port.ctx);
  }
  return lo + (link->port.random_u32(link->port.ctx) % (span + 1u));
}

static uint32_t draw_backoff_ms(boomlink_link_t *link) {
  return draw_in_range_ms(link, link->config.backoff_min_ms, link->config.backoff_max_ms);
}

/**
 * Section 9.7's pre-transmission jitter, uniform over [0, tx_jitter_max_ms].
 *
 * The delay backoff cannot substitute for. Backoff separates nodes that have
 * ALREADY collided; this separates them before the collision, which is the
 * situation 9.7 actually describes - several nodes detecting one gunshot within
 * milliseconds, each transmitting immediately. Without it the first attempt
 * collides every time and the retry is the only thing that ever spreads them
 * out, which costs an ACK timeout per event on every node involved.
 *
 * Zero means disabled, and that is a real configuration rather than a
 * placeholder: a gateway that is the only transmitter in its own conversation
 * has nothing to collide with and no reason to add latency.
 */
static uint32_t draw_jitter_ms(boomlink_link_t *link) {
  if (link->config.tx_jitter_max_ms == 0u) {
    return 0u;
  }
  return draw_in_range_ms(link, 0u, link->config.tx_jitter_max_ms);
}

/**
 * Release the pipeline and report the outcome (section 9.6).
 *
 * `rssi_dbm`/`snr_db` are the ACKing frame's OWN signal quality for
 * BOOMLINK_TX_ACKED, 0.0f for the other two outcomes - there is no incoming
 * packet to report one for, and 0.0f is this codebase's existing "nothing to
 * report" sentinel for signal quality (see boomlink_link_stats_t.last_rssi_dbm
 * and the RX side's own "A received nothing and reports no RSSI" test). Passed
 * through rather than read back from link->stats.last_rssi_dbm for the exact
 * reason section 9.2's RX side takes rssi/snr as parameters instead of reading
 * that same field: a caller relying on the "last" stats value instead would
 * get a stale reading the moment more than one packet is drained in the same
 * boomlink_link_poll() call and the ACK isn't the last of them - the same
 * misattribution the RX callback's rssi_dbm/snr_db parameters exist to
 * prevent, just needing a burst that buries the ACK instead of a burst of
 * payloads. Once genuinely invisible on the real target because radio.h was
 * single-slot (see boomlink_link_poll()'s own comment on that history); not
 * anymore - Phase C gave radio.h a real multi-packet ring, the same kind the
 * fake port this package tests against already had, so a burst that buries
 * an ACK is now a real, reachable path on hardware too, not just in tests.
 */
static void finish_tx(boomlink_link_t *link, boomlink_tx_outcome_t outcome, float rssi_dbm,
                      float snr_db) {
  if (outcome == BOOMLINK_TX_NO_ACK) {
    link->stats.tx_failures++;
  }
  const uint32_t destination = link->tx.header.destination_id;
  const uint32_t sequence    = link->tx.header.sequence;
  const uint8_t  attempts    = link->tx.attempts;

  /* Cleared BEFORE the callback, not after. A callback that queues its next frame
     - a retry policy one layer up, a command response - is an ordinary thing to
     write, and it must not observe a pipeline that is half torn down. */
  memset(&link->tx, 0, sizeof(link->tx));
  link->tx.state = BOOMLINK_TX_STATE_IDLE;

  if (link->config.on_tx_done != NULL) {
    link->config.on_tx_done(link->config.on_tx_done_user, outcome, destination, sequence,
                            attempts, rssi_dbm, snr_db);
  }
}

/** Put the held frame on the air once. */
static void attempt_tx(boomlink_link_t *link, uint32_t now) {
  uint8_t      frame[BOOMLINK_LINKFRAME_HEADER_SIZE + BOOMLINK_TX_MAX_PAYLOAD];
  const size_t len = BOOMLINK_LINKFRAME_HEADER_SIZE + link->tx.item.payload_len;
  boomlink_linkframe_encode(&link->tx.header, frame);
  if (link->tx.item.payload_len > 0u) {
    memcpy(&frame[BOOMLINK_LINKFRAME_HEADER_SIZE], link->tx.item.payload,
           link->tx.item.payload_len);
  }

  if (!transmit(link, frame, len)) {
    /* The radio refused - busy, or absent. The frame STAYS here, with the
       sequence it was already assigned, and the next poll tries again. This is
       the whole reason the pipeline holds a frame instead of transmitting
       straight out of the queue: an earlier version popped first and dropped the
       item on a refused send, so a busy radio silently destroyed queued traffic
       while reporting nothing but a counter.
       Not counted as an attempt, and NOT bounded by a refusal count. A count
       would be the wrong shape: radio.h reports "busy" for the whole duration of
       a transmission in progress, so at SF12 a single frame legitimately collects
       thousands of refusals from a superloop polling every millisecond, and any
       bound low enough to catch a dead radio would shed live traffic constantly.
       A radio that refuses forever is a dead link whichever way this branch is
       written; what matters is that it is VISIBLE, which is what tx_failures
       does. */
    return;
  }

  link->tx.attempts++;
  if (link->tx.attempts == 1u) {
    link->stats.tx_envelopes++;
  } else {
    /* Section 9.10 counts retries apart from envelopes, so the two never
       double-count: transmissions on the air is their sum. */
    link->stats.tx_retries++;
  }

  if (!link->tx.item.ack_requested) {
    /* Nothing to wait for. Section 9.6's retry machinery is scoped to unicast
       frames with ack_requested set, and a broadcast never has it (section 9.9),
       so this is also the only path a broadcast can take. */
    finish_tx(link, BOOMLINK_TX_SENT, 0.0f, 0.0f); /* nothing was received */
    return;
  }
  link->tx.state         = BOOMLINK_TX_STATE_WAIT_ACK;
  link->tx.sent_at_ms    = now;
  link->tx.ack_window_ms = ack_window_ms(link, len);
}

/** Take the next queued frame into the pipeline, assigning its sequence. */
static void dequeue_tx(boomlink_link_t *link) {
  if (!boomlink_txqueue_pop(&link->txqueue, &link->tx.item)) {
    return;
  }
  boomlink_linkframe_header_init(&link->tx.header);
  link->tx.header.magic          = link->config.magic;
  link->tx.header.frame_type     = BOOMLINK_FRAME_TYPE_DATA;
  link->tx.header.destination_id = link->tx.item.destination_id;
  link->tx.header.source_id      = link->config.node_id;
  link->tx.header.session_id     = link->session_id;
  link->tx.header.ack_requested  = link->tx.item.ack_requested;
  /* Section 9.1: assigned at DEQUEUE, so the on-air sequence stays monotonic per
     session however the priority queue reordered things - and assigned ONCE, so
     every retransmission of this frame carries it unchanged (section 9.6), which
     is what lets the receiver suppress the duplicate. */
  link->tx.header.sequence = link->next_sequence++;
  link->tx.attempts        = 0u;

  /* Section 9.7's TX jitter, before the first attempt only - a retransmission
     gets the backoff instead. The sequence is assigned BEFORE the delay, not
     after: 9.1 puts it at dequeue, and a frame that has left the queue has left
     it whether or not it is on the air yet. */
  const uint32_t jitter = draw_jitter_ms(link);
  if (jitter > 0u) {
    link->tx.state            = BOOMLINK_TX_STATE_JITTER;
    link->tx.delay_started_ms = link->port.now_ms(link->port.ctx);
    link->tx.delay_ms         = jitter;
    return;
  }
  link->tx.state = BOOMLINK_TX_STATE_READY;
}

/** One step of section 9.6's stop-and-wait pipeline. */
static void service_tx(boomlink_link_t *link) {
  const uint32_t now = link->port.now_ms(link->port.ctx);

  if (link->tx.state == BOOMLINK_TX_STATE_WAIT_ACK) {
    if (boomlink_elapsed_ms(link->tx.sent_at_ms, now) < link->tx.ack_window_ms) {
      /* Still inside the window. The queue is held: the radio is half-duplex, and
         transmitting anything now would mean not hearing the ACK (section 9.6). */
      return;
    }
    /* Section 9.6's "do not retry forever". attempts counts transmissions the
       radio accepted, so max_attempts is a budget of transmissions and not of
       timeouts - three attempts means three frames on the air, two retries. */
    if (link->tx.attempts >= link->config.max_attempts) {
      finish_tx(link, BOOMLINK_TX_NO_ACK, 0.0f, 0.0f); /* nothing was received */
      return;
    }
    link->tx.state            = BOOMLINK_TX_STATE_BACKOFF;
    link->tx.delay_started_ms = now;
    link->tx.delay_ms         = draw_backoff_ms(link);
    /* Deliberately returns rather than falling through to the retransmission: a
       zero-length backoff is legal (equal bounds), and retransmitting in the same
       poll would make the state unobservable and the delay untestable. One extra
       superloop iteration costs nothing at this traffic rate. */
    return;
  }

  /* Both of section 9.7's delays, checked together because the arithmetic is the
     same and only the reason for waiting differs. Kept as separate states so a
     diagnostic can say which one it is - see boomlink_tx_state_t. */
  if (link->tx.state == BOOMLINK_TX_STATE_BACKOFF ||
      link->tx.state == BOOMLINK_TX_STATE_JITTER) {
    if (boomlink_elapsed_ms(link->tx.delay_started_ms, now) < link->tx.delay_ms) {
      return;
    }
    link->tx.state = BOOMLINK_TX_STATE_READY;
  }

  if (link->tx.state == BOOMLINK_TX_STATE_IDLE) {
    dequeue_tx(link);
  }

  if (link->tx.state == BOOMLINK_TX_STATE_READY) {
    attempt_tx(link, now);
  }
}

void boomlink_link_poll(boomlink_link_t *link) {
  size_t len  = 0u;
  float  rssi = 0.0f;
  float  snr  = 0.0f;
  /* Drain, not one-per-poll: the radio's own model is a small fixed-depth
     ring, not unbounded (see radio.h - Phase C), so a packet arriving once
     that ring is already full is lost either way - but a caller that polls
     once per superloop iteration must not fall behind a burst smaller than
     the ring that it could have caught by draining here instead.
     The cap offered is the PORT's, not the buffer's: they differ on a reduced
     radio profile, and offering more than the radio can produce would leave the
     oversize check comparing against a limit no real packet can exceed. */
  while (link->port.poll_rx(link->port.ctx, link->rx_buffer, link->port.max_packet, &len,
                            &rssi, &snr)) {
    handle_packet(link, len, rssi, snr);
  }

  service_tx(link);
}

boomlink_link_send_result_t boomlink_link_send(boomlink_link_t *link,
                                               uint32_t destination_id,
                                               boomlink_tx_priority_t priority,
                                               bool request_ack, const uint8_t *payload,
                                               size_t payload_len) {
  /* Section 9.1's "enforce destination rules". Broadcast is fine; these two are
     not. The unconfigured address is nobody, and our own ID would put a frame on
     the air that the receiving half of this very node would reject as an
     impossible source - after paying for the airtime. */
  if (destination_id == BOOMLINK_ADDR_INVALID || destination_id == link->config.node_id) {
    link->stats.tx_dropped++;
    return BOOMLINK_LINK_SEND_BAD_DESTINATION;
  }

  /* Section 7.3: "an oversized frame is rejected before transmission" - and
     rejected HERE, where the caller finds out, rather than at the radio where the
     only possible outcome is a silent drop. Checked against this port's
     max_packet, which on a reduced radio profile is tighter than the queue's
     compile-time slot size: without this, a payload that fits a slot but not the
     radio would be queued, reported as accepted, and destroyed later. */
  if (payload_len > link->port.max_packet - BOOMLINK_LINKFRAME_HEADER_SIZE) {
    link->stats.tx_dropped++;
    return BOOMLINK_LINK_SEND_TOO_LONG;
  }

  /* Section 9.9: "broadcast never requests link ACK". Forced rather than
     rejected - the rule exists to prevent an ACK storm, and a caller
     broadcasting a detection event should not have to know it. */
  const bool ack = request_ack && destination_id != BOOMLINK_ADDR_BROADCAST;
  switch (boomlink_txqueue_push(&link->txqueue, destination_id, priority, ack, payload,
                                payload_len)) {
    case BOOMLINK_TXQUEUE_OK:
      return BOOMLINK_LINK_SEND_OK;
    case BOOMLINK_TXQUEUE_OK_EVICTED:
      /* Counted apart from a drop: shedding telemetry to make room for a
         detection is section 9.8's policy working, not a fault. */
      link->stats.tx_shed++;
      return BOOMLINK_LINK_SEND_OK_EVICTED;
    case BOOMLINK_TXQUEUE_TOO_LONG:
      /* Unreachable via the port check above, which is tighter or equal - kept
         because the queue owns its own bound and this maps it rather than assuming
         the two can never disagree. */
      link->stats.tx_dropped++;
      return BOOMLINK_LINK_SEND_TOO_LONG;
    case BOOMLINK_TXQUEUE_FULL:
    default:
      link->stats.tx_dropped++;
      return BOOMLINK_LINK_SEND_QUEUE_FULL;
  }
}

/* These three are the engine's diagnostics surface, and the only functions here
   that tolerate a NULL link. Everything else is called from the superloop with a
   link the caller just brought up; these are called from a CLI or a telemetry
   builder, where a wrong pointer should cost a missing reading rather than a hard
   fault in the field - the same policy radio.h states for itself. */
void boomlink_link_get_stats(const boomlink_link_t *link, boomlink_link_stats_t *out) {
  if (out == NULL) {
    return;
  }
  if (link == NULL) {
    memset(out, 0, sizeof(*out));
    return;
  }
  *out = link->stats;
}

void boomlink_link_reset_stats(boomlink_link_t *link) {
  if (link == NULL) {
    return;
  }
  /* Only the counters. Not memset(link, 0, sizeof(*link)) - see the header: that
     would zero the session and sequence inside a live conversation, and the peer
     would suppress everything this node then sent as a replay of sequences its
     duplicate window had already accepted. */
  memset(&link->stats, 0, sizeof(link->stats));
}

uint32_t boomlink_link_session_id(const boomlink_link_t *link) {
  return link != NULL ? link->session_id : 0u;
}

boomlink_tx_state_t boomlink_link_tx_state(const boomlink_link_t *link) {
  return link != NULL ? link->tx.state : BOOMLINK_TX_STATE_IDLE;
}

size_t boomlink_link_queue_depth(const boomlink_link_t *link) {
  return link != NULL ? boomlink_txqueue_count(&link->txqueue) : 0u;
}
