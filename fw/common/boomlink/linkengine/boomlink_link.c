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
  /* Section 9.6 says "do not retry forever"; zero attempts is the opposite
     mistake and would mean queued frames are never sent at all. */
  if (config->max_attempts == 0u) {
    return false;
  }
  if (config->backoff_max_ms < config->backoff_min_ms) {
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

/** Transmit one already-encoded frame, accumulating airtime and failures. */
static bool transmit(boomlink_link_t *link, const uint8_t *frame, size_t len) {
  if (len > link->port.max_packet) {
    /* Section 7.3: "An oversized frame is rejected before transmission". The
       frame layer cannot make this check - it has no radio dependency and so
       cannot know the limit - which is why it lands here. */
    link->stats.tx_failures++;
    return false;
  }
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
    /* Nothing is pending yet in this build - the stop-and-wait state arrives with
       the TX pipeline - so every ACK addressed to us is currently unmatched. */
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
    link->config.on_rx(link->config.on_rx_user, header.source_id,
                       &link->rx_buffer[BOOMLINK_LINKFRAME_HEADER_SIZE], payload_len);
  }
}

/** Dequeue one frame and put it on the air. */
static void service_tx(boomlink_link_t *link) {
  boomlink_txqueue_item_t item;
  if (!boomlink_txqueue_pop(&link->txqueue, &item)) {
    return;
  }

  boomlink_linkframe_header_t header;
  boomlink_linkframe_header_init(&header);
  header.magic          = link->config.magic;
  header.frame_type     = BOOMLINK_FRAME_TYPE_DATA;
  header.destination_id = item.destination_id;
  header.source_id      = link->config.node_id;
  header.session_id     = link->session_id;
  header.ack_requested  = item.ack_requested;
  /* Section 9.1: assigned at DEQUEUE, so the on-air sequence stays monotonic per
     session however the priority queue reordered things. */
  header.sequence = link->next_sequence++;

  uint8_t frame[BOOMLINK_LINKFRAME_HEADER_SIZE + BOOMLINK_TX_MAX_PAYLOAD];
  boomlink_linkframe_encode(&header, frame);
  if (item.payload_len > 0u) {
    memcpy(&frame[BOOMLINK_LINKFRAME_HEADER_SIZE], item.payload, item.payload_len);
  }
  if (transmit(link, frame, BOOMLINK_LINKFRAME_HEADER_SIZE + item.payload_len)) {
    link->stats.tx_envelopes++;
  }
}

void boomlink_link_poll(boomlink_link_t *link) {
  size_t len  = 0u;
  float  rssi = 0.0f;
  float  snr  = 0.0f;
  /* Drain, not one-per-poll: the radio's own model is single-slot (see
     radio.h), so a second packet arriving before this returns is lost either
     way - but a caller that polls once per superloop iteration must not fall
     permanently behind a burst it could have caught.
     The cap offered is the PORT's, not the buffer's: they differ on a reduced
     radio profile, and offering more than the radio can produce would leave the
     oversize check comparing against a limit no real packet can exceed. */
  while (link->port.poll_rx(link->port.ctx, link->rx_buffer, link->port.max_packet, &len,
                            &rssi, &snr)) {
    handle_packet(link, len, rssi, snr);
  }

  service_tx(link);
}

boomlink_txqueue_result_t boomlink_link_send(boomlink_link_t *link, uint32_t destination_id,
                                             boomlink_tx_priority_t priority,
                                             bool request_ack, const uint8_t *payload,
                                             size_t payload_len) {
  /* Section 9.9: "broadcast never requests link ACK". Forced rather than
     rejected - the rule exists to prevent an ACK storm, and a caller
     broadcasting a detection event should not have to know it. */
  const bool ack = request_ack && destination_id != BOOMLINK_ADDR_BROADCAST;
  return boomlink_txqueue_push(&link->txqueue, destination_id, priority, ack, payload,
                               payload_len);
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
