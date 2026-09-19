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

/* Section 7.3's reservation, as currently specified: bits 2-7 unassigned. The
   reserved mask is derived from the assigned one (see the header), so the two
   cannot contradict each other and this is not checking that - it pins the WIRE
   FORMAT. Assigning a flag bit changes what a v1 receiver must tolerate, so it
   has to be a deliberate set of edits - this assert, the assigned mask in the
   header, boomlink.md section 7.3 and the Python mirror - not a side effect of
   adding a macro.
   What no assert here can catch: a new BOOMLINK_LINKFRAME_FLAG_X defined without
   being added to BOOMLINK_LINKFRAME_FLAGS_ASSIGNED_MASK. The mask would stay
   0xFC, this assert would pass, and the parser would report the newly assigned
   bit as unrecognized. The two things that do push back are that the flag and
   the mask sit on adjacent lines in the header, and that the cross-language test
   compares both masks against the Python reference - so the mirror has to move
   in lockstep even though the C alone would not notice. */
_Static_assert(BOOMLINK_LINKFRAME_FLAGS_RESERVED_MASK == 0xFCu,
               "flags bits 2-7 are the reserved ones per boomlink.md section 7.3; assigning "
               "one is a wire-format change - update the spec and the Python mirror with it");

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
                               uint8_t out[static BOOMLINK_LINKFRAME_HEADER_SIZE]) {
  out[OFF_MAGIC] = header->magic;
  /* Both halves masked to a nibble, but only one of the two masks can actually
     change this byte, and it is worth knowing which: frame_type's is
     load-bearing - it is OR'd into the low nibble, so a value above 15 would
     otherwise raise the version nibble and turn a bad type into a bad VERSION
     (or, worse, into a different valid frame). version's cannot: `<< 4` followed
     by the truncation to uint8_t already discards exactly the bits the mask
     removes, for every possible input. It stays because the expression should
     say what it means, and so the field can be widened or moved without the
     safety silently depending on that coincidence. */
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
    boomlink_linkframe_header_t out_header[BOOMLINK_LINKFRAME_ONE],
    size_t out_payload_len[BOOMLINK_LINKFRAME_ONE]) {
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

/* Section 7.2's address space, in one place: real node IDs are
   0x00000001..0xFFFFFFFE, with 0 meaning unconfigured and 0xFFFFFFFF reserved
   for broadcast. Both ends matter and both have a distinct failure: a
   factory-fresh node whose id is still 0 would "match" a frame addressed to 0,
   and a node misconfigured to the broadcast address would match every broadcast
   frame and answer on behalf of the whole network. Used by both callers below so
   that range is not written out twice. */
static bool is_valid_node_id(uint32_t node_id) {
  return node_id != BOOMLINK_ADDR_INVALID && node_id != BOOMLINK_ADDR_BROADCAST;
}

bool boomlink_linkframe_is_for_node(uint32_t destination_id, uint32_t local_node_id) {
  /* A node whose own address is not a valid node ID accepts nothing at all,
     broadcast included - see the header's contract. Checked FIRST so the two
     tests below cannot accidentally let such a node act on traffic. */
  if (!is_valid_node_id(local_node_id)) {
    return false;
  }
  return destination_id == local_node_id || destination_id == BOOMLINK_ADDR_BROADCAST;
}

BOOMLINK_LINKFRAME_MUST_CHECK
bool boomlink_linkframe_make_ack(const boomlink_linkframe_header_t received[static 1],
                                 uint32_t local_node_id,
                                 boomlink_linkframe_header_t out_ack[static 1]) {
  /* Every value this function needs is copied out of `received` BEFORE `out_ack`
     is touched, so `make_ack(&h, id, &h)` works. Reading them afterwards would
     make the in-place form fail in the worst possible way: the memset would zero
     source_id, the guard below would see address 0, and the caller would get
     `false` plus a wiped header - indistinguishable from "the peer's source
     address was unusable", which is the most misleading diagnosis available. An
     engine on a 20 KB part has an obvious reason to reuse one header, so this is
     ordered rather than merely documented. */
  const uint32_t ack_destination = received->source_id;
  const uint8_t  ack_magic       = received->magic;
  const uint32_t ack_session     = received->session_id;
  const uint32_t ack_sequence    = received->sequence;

  /* Zeroed, so a refusal below leaves nothing partially built and so every field
     section 9.5 does not name is 0 without being listed. That is load-bearing,
     not tidiness: fragment_index, both known flags and the reserved bits are
     never assigned on the success path, so this line is the only thing that
     clears them - which is what makes "an ACK never requests another ACK"
     satisfied by construction, and what stops an engine reusing one static
     header from emitting the previous frame's flags. */
  memset(out_ack, 0, sizeof(*out_ack));

  /* Both ends of the ACK's addressing have to be real nodes: the destination
     comes from the received frame, which this layer never validated (a source
     address is not the parser's business), and the source is configuration. See
     the header for why this one check belongs here while "should I ACK at all"
     does not. */
  if (!is_valid_node_id(ack_destination) || !is_valid_node_id(local_node_id)) {
    return false;
  }

  out_ack->magic      = ack_magic;
  out_ack->version    = BOOMLINK_LINKFRAME_VERSION;
  out_ack->frame_type = BOOMLINK_FRAME_TYPE_ACK;
  /* The swap that section 9.5 is really about: the ACK goes back to whoever
     SENT the frame, not to whoever it was addressed to (which is this node). */
  out_ack->destination_id = ack_destination;
  out_ack->source_id      = local_node_id;
  /* Copied unchanged - this pair is what the original sender matches the ACK
     against, so transposing them silently breaks every delivery. */
  out_ack->session_id = ack_session;
  out_ack->sequence   = ack_sequence;
  /* ack_requested stays false: section 9.5's "ACK packets never request another
     ACK", satisfied by construction rather than by the engine remembering to. */
  return true;
}

BOOMLINK_LINKFRAME_MUST_CHECK
bool boomlink_linkframe_ack_matches(const boomlink_linkframe_header_t pending[static 1],
                                    const boomlink_linkframe_header_t ack[static 1],
                                    uint32_t local_node_id) {
  /* BOTH ends must be real node IDs before any field is compared, and both
     guards are load-bearing against a forged ACK rather than a tidy-up:

     local_node_id - an unconfigured or broadcast-addressed node has no pending
     frames of its own, and without this an ACK addressed to 0xFFFFFFFF
     "matches" at a node that thinks it IS 0xFFFFFFFF.

     pending->destination_id - a broadcast frame is never acknowledged (section
     9.9), so one should never be in the ACK-pending slot at all. But if it is,
     an ACK forged with source_id = 0xFFFFFFFF satisfies
     `ack->source_id == pending->destination_id` and every other condition below,
     so it would match. make_ack() cannot build that ACK, which is exactly why
     trusting that nobody sends it would be wrong - a non-compliant or hostile
     peer can. Found by the near-miss vectors in test_linkframe.py, not by
     reasoning: the first version of this function omitted this guard and
     claimed in its comment that no special case was needed. */
  if (!is_valid_node_id(local_node_id) || !is_valid_node_id(pending->destination_id)) {
    return false;
  }
  /* Every one of these is load-bearing, and the ones that look redundant are
     the important ones - see the header. Dropping the two address comparisons
     leaves a matcher that accepts another node's ACK for its own traffic while
     every delivery test still passes. */
  return ack->frame_type == BOOMLINK_FRAME_TYPE_ACK &&
         ack->session_id == pending->session_id && ack->sequence == pending->sequence &&
         ack->source_id == pending->destination_id && ack->destination_id == local_node_id;
}

void boomlink_linkframe_header_init(boomlink_linkframe_header_t out_header[static 1]) {
  memset(out_header, 0, sizeof(*out_header));
  out_header->magic      = BOOMLINK_LINKFRAME_MAGIC_DEFAULT;
  out_header->version    = BOOMLINK_LINKFRAME_VERSION;
  out_header->frame_type = BOOMLINK_FRAME_TYPE_DATA;
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
    /* Not a result - listed only because the switch has no default:, and it is
       a real enumerator. Falls through to the string below, which is the right
       answer for it. */
    case BOOMLINK_LINKFRAME_RESULT_COUNT:          break;
  }
  /* No default: above, so adding an enumerator without a string is a
     -Wswitch warning (and an error under BOOMLINK_WERROR) rather than a
     silent fall-through here. */
  return "unknown parse result";
}
