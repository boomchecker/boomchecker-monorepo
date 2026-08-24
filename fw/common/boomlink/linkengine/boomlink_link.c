/**
 ******************************************************************************
 * @file    boomlink_link.c
 ******************************************************************************
 */
#include "boomlink_link.h"

#include <string.h>

_Static_assert(sizeof(((boomlink_link_t *)0)->rx_buffer) >= BOOMLINK_LINKFRAME_HEADER_SIZE,
               "the RX buffer cannot hold a bare header");

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
    /* The frame asked for an ACK, was valid, and was for us - but its source
       address cannot be acknowledged (section 7.2). Counted separately because
       neither the malformed nor the wrong-destination counter covers it; see the
       gap recorded in section 9.10. */
    link->stats.ack_unaddressable++;
    return;
  }
  uint8_t bytes[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&ack, bytes);
  if (transmit(link, bytes, sizeof(bytes))) {
    link->stats.ack_sent++;
  }
}

/** One received packet, through section 9.2's pipeline. */
static void handle_packet(boomlink_link_t *link, size_t len, float rssi, float snr) {
  link->stats.last_rssi_dbm = rssi;
  link->stats.last_snr_db   = snr;

  if (len > sizeof(link->rx_buffer)) {
    /* Longer than the radio's own ceiling, so only part of it was copied and the
       rest is unknowable. Cannot be a frame this network produced; counted
       separately from malformed so a flood of these is recognisable as a foreign
       transmitter or a driver fault rather than as corrupt traffic. */
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
    /* Section 9.2 matches ACKs before validating the destination, which is safe
       because the matcher requires the ACK to be addressed to this node anyway.
       Nothing is pending yet in this build - the stop-and-wait state arrives with
       the TX pipeline - so every ACK is currently unmatched. */
    link->stats.ack_unmatched++;
    return;
  }

  /* Section 7.2's acceptance rule, via the frame layer so the engine cannot
     drift from it. */
  if (!boomlink_linkframe_is_for_node(header.destination_id, link->config.node_id)) {
    link->stats.rx_other_destination++;
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
     permanently behind a burst it could have caught. */
  while (link->port.poll_rx(link->port.ctx, link->rx_buffer, sizeof(link->rx_buffer), &len,
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

void boomlink_link_get_stats(const boomlink_link_t *link, boomlink_link_stats_t *out) {
  *out = link->stats;
}

void boomlink_link_reset_stats(boomlink_link_t *link) {
  memset(&link->stats, 0, sizeof(link->stats));
}

uint32_t boomlink_link_session_id(const boomlink_link_t *link) {
  return link->session_id;
}
