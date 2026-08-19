/**
 ******************************************************************************
 * @file    boomlink_linkframe.c
 ******************************************************************************
 */
#include "boomlink_linkframe.h"

#include <string.h> /* memset */

/* Byte offsets from boomlink.md section 7.3, named rather than inlined so the
   encoder and parser cannot drift apart on a field's position. */
#define OFF_MAGIC          0u
#define OFF_VERSION_TYPE   1u
#define OFF_FLAGS          2u
#define OFF_FRAGMENT_INDEX 3u
#define OFF_DESTINATION    4u
#define OFF_SOURCE         8u
#define OFF_SESSION        12u
#define OFF_SEQUENCE       16u

_Static_assert(OFF_SEQUENCE + 4u == BOOMLINK_LINKFRAME_HEADER_SIZE,
               "the field offsets above no longer add up to the declared header size");
_Static_assert(BOOMLINK_LINKFRAME_VERSION <= 0x0Fu,
               "version must fit the high nibble of byte 1");
_Static_assert(BOOMLINK_FRAME_TYPE_DATA <= 0x0Fu && BOOMLINK_FRAME_TYPE_ACK <= 0x0Fu,
               "frame type must fit the low nibble of byte 1");

/* The reserved mask must be exactly the flag bits that are NOT assigned:
   together the three cover the whole byte, and the assigned bits must not
   appear in the mask. Without this, assigning a future FLAG_X = 0x04 without
   clearing that bit from the mask would leave the parser reporting an already
   assigned bit as "reserved/unrecognized" - a wrong diagnostic in the one field
   whose purpose is to say what a newer peer sent that we do not understand. */
_Static_assert(((BOOMLINK_LINKFRAME_FLAG_ACK_REQUESTED |
                 BOOMLINK_LINKFRAME_FLAG_MORE_FRAGMENTS |
                 BOOMLINK_LINKFRAME_FLAGS_RESERVED_MASK) == 0xFFu) &&
                   (((BOOMLINK_LINKFRAME_FLAG_ACK_REQUESTED |
                      BOOMLINK_LINKFRAME_FLAG_MORE_FRAGMENTS) &
                     BOOMLINK_LINKFRAME_FLAGS_RESERVED_MASK) == 0u),
               "the reserved flags mask is no longer exactly the unassigned flag bits - a "
               "newly assigned bit must be removed from BOOMLINK_LINKFRAME_FLAGS_RESERVED_MASK");

/* Explicit little-endian access, one byte at a time. Not a memcpy of a
   uint32_t and not a packed struct: the wire format is fixed by the spec and
   must not depend on the host being little-endian, on alignment, or on the
   compiler's struct padding. The cost is irrelevant at 20 bytes per packet. */
static void put_u32_le(uint8_t *buf, uint32_t value) {
  buf[0] = (uint8_t)(value & 0xFFu);
  buf[1] = (uint8_t)((value >> 8) & 0xFFu);
  buf[2] = (uint8_t)((value >> 16) & 0xFFu);
  buf[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static uint32_t get_u32_le(const uint8_t *buf) {
  return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) |
         ((uint32_t)buf[3] << 24);
}

void boomlink_linkframe_encode(const boomlink_linkframe_header_t *header,
                               uint8_t out[BOOMLINK_LINKFRAME_HEADER_BOUND]) {
  out[OFF_MAGIC] = header->magic;
  /* Both halves masked to a nibble: a caller passing a too-large version or
     frame type would otherwise corrupt the neighbouring field silently. */
  out[OFF_VERSION_TYPE] =
      (uint8_t)(((header->version & 0x0Fu) << 4) | (header->frame_type & 0x0Fu));

  uint8_t flags = 0u;
  if (header->ack_requested) {
    flags |= BOOMLINK_LINKFRAME_FLAG_ACK_REQUESTED;
  }
  if (header->more_fragments) {
    flags |= BOOMLINK_LINKFRAME_FLAG_MORE_FRAGMENTS;
  }
  /* header->reserved_flags is deliberately NOT copied through - section 7.3
     requires bits 2-7 to be sent as 0 until a future PR assigns them. Echoing
     back whatever a parsed frame happened to carry would put unassigned bits
     on the air, which is exactly what the reservation exists to prevent. */
  out[OFF_FLAGS] = flags;

  out[OFF_FRAGMENT_INDEX] = header->fragment_index;
  put_u32_le(&out[OFF_DESTINATION], header->destination_id);
  put_u32_le(&out[OFF_SOURCE], header->source_id);
  put_u32_le(&out[OFF_SESSION], header->session_id);
  put_u32_le(&out[OFF_SEQUENCE], header->sequence);
}

boomlink_linkframe_parse_result_t boomlink_linkframe_parse(
    const uint8_t *buf, size_t len, uint8_t expected_magic,
    boomlink_linkframe_header_t *out_header, size_t *out_payload_len) {
  /* Zero the outputs up front so every early return leaves the caller with a
     definitively empty header rather than a half-filled one. */
  memset(out_header, 0, sizeof(*out_header));
  *out_payload_len = 0u;

  if (len < BOOMLINK_LINKFRAME_HEADER_SIZE) {
    return BOOMLINK_LINKFRAME_ERR_TOO_SHORT;
  }

  /* Magic and version first, before anything else is even read out of the
     buffer - section 7.3 requires foreign or unknown-version traffic to be
     dropped "before any further processing". */
  const uint8_t magic = buf[OFF_MAGIC];
  if (magic != expected_magic) {
    return BOOMLINK_LINKFRAME_ERR_MAGIC;
  }
  const uint8_t version    = (uint8_t)((buf[OFF_VERSION_TYPE] >> 4) & 0x0Fu);
  const uint8_t frame_type = (uint8_t)(buf[OFF_VERSION_TYPE] & 0x0Fu);
  if (version != BOOMLINK_LINKFRAME_VERSION) {
    return BOOMLINK_LINKFRAME_ERR_VERSION;
  }
  if (frame_type != BOOMLINK_FRAME_TYPE_DATA && frame_type != BOOMLINK_FRAME_TYPE_ACK) {
    return BOOMLINK_LINKFRAME_ERR_FRAME_TYPE;
  }

  const uint8_t flags          = buf[OFF_FLAGS];
  const uint8_t fragment_index = buf[OFF_FRAGMENT_INDEX];
  const bool    more_fragments = (flags & BOOMLINK_LINKFRAME_FLAG_MORE_FRAGMENTS) != 0u;

  /* Both fields, not just more_fragments - see the header's comment on
     BOOMLINK_LINKFRAME_ERR_FRAGMENTED for why checking one is unsafe. */
  if (more_fragments || fragment_index != 0u) {
    return BOOMLINK_LINKFRAME_ERR_FRAGMENTED;
  }

  const size_t payload_len = len - BOOMLINK_LINKFRAME_HEADER_SIZE;
  if (frame_type == BOOMLINK_FRAME_TYPE_ACK && payload_len != 0u) {
    return BOOMLINK_LINKFRAME_ERR_ACK_HAS_PAYLOAD;
  }

  out_header->magic          = magic;
  out_header->version        = version;
  out_header->frame_type     = frame_type;
  out_header->ack_requested  = (flags & BOOMLINK_LINKFRAME_FLAG_ACK_REQUESTED) != 0u;
  out_header->more_fragments = more_fragments;
  out_header->fragment_index = fragment_index;
  out_header->reserved_flags = (uint8_t)(flags & BOOMLINK_LINKFRAME_FLAGS_RESERVED_MASK);
  out_header->destination_id = get_u32_le(&buf[OFF_DESTINATION]);
  out_header->source_id      = get_u32_le(&buf[OFF_SOURCE]);
  out_header->session_id     = get_u32_le(&buf[OFF_SESSION]);
  out_header->sequence       = get_u32_le(&buf[OFF_SEQUENCE]);
  *out_payload_len           = payload_len;
  return BOOMLINK_LINKFRAME_OK;
}

bool boomlink_linkframe_is_for_node(uint32_t destination_id, uint32_t local_node_id) {
  /* An unconfigured node accepts nothing at all, broadcast included - see the
     header's contract. Checked first so the broadcast test below cannot
     accidentally let a half-provisioned node act on traffic. */
  if (local_node_id == BOOMLINK_ADDR_INVALID) {
    return false;
  }
  return destination_id == local_node_id || destination_id == BOOMLINK_ADDR_BROADCAST;
}

const char *boomlink_linkframe_parse_result_str(boomlink_linkframe_parse_result_t result) {
  switch (result) {
    case BOOMLINK_LINKFRAME_OK:                    return "ok";
    case BOOMLINK_LINKFRAME_ERR_TOO_SHORT:         return "shorter than a link frame header";
    case BOOMLINK_LINKFRAME_ERR_MAGIC:             return "wrong magic / network ID";
    case BOOMLINK_LINKFRAME_ERR_VERSION:           return "unsupported link frame version";
    case BOOMLINK_LINKFRAME_ERR_FRAME_TYPE:        return "unknown frame type";
    case BOOMLINK_LINKFRAME_ERR_FRAGMENTED:        return "fragmented frame (unsupported)";
    case BOOMLINK_LINKFRAME_ERR_ACK_HAS_PAYLOAD:   return "ACK frame carrying a payload";
  }
  /* No default: above, so adding an enumerator without a string is a
     -Wswitch warning (and an error under BOOMLINK_WERROR) rather than a
     silent fall-through here. */
  return "unknown parse result";
}
