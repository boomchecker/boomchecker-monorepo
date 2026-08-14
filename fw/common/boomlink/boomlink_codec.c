/**
 ******************************************************************************
 * @file    boomlink_codec.c
 ******************************************************************************
 */
#include "boomlink_codec.h"

#include <pb_decode.h>
#include <pb_encode.h>

/* boomlink_Envelope_size (envelope.pb.h) is Nanopb's own worst-case encoded
   size for the current schema, computed from every bounded field in
   nanopb/boomlink.options. Comparing it against the real on-air budget here -
   RADIO_MAX_PAYLOAD (fw/bom-stm32node/App/radio/radio.h) minus the link frame
   header (boomlink.md section 7.3, 20 bytes, landing in PR 3) - at compile
   time means growing a bounded field enough to blow that budget is a build
   failure in this file, not a runtime surprise the first time a real
   Envelope is too big to fit in one LoRa packet. 255 is duplicated rather
   than included from radio.h: this package must not depend on
   bom-stm32node's App/ layer (see boomlink.md section 4 repository rules). */
#define BOOMLINK_RADIO_MAX_PAYLOAD 255
#define BOOMLINK_LINK_FRAME_HEADER_SIZE 20
#define BOOMLINK_ENVELOPE_BUDGET (BOOMLINK_RADIO_MAX_PAYLOAD - BOOMLINK_LINK_FRAME_HEADER_SIZE)

_Static_assert(boomlink_Envelope_size <= BOOMLINK_ENVELOPE_BUDGET,
               "worst-case Envelope encoding no longer fits the LoRa link frame payload "
               "budget - shrink a bounded field in nanopb/boomlink.options");

/* header.proto: "protocol_version: BoomProtocol compatibility version,
   initially 1" - 0 is proto3's default for an omitted scalar, so it means
   "the caller/peer never set this field" rather than a real version,
   exactly like a missing `has_header`. */
static bool has_valid_header(const boomlink_Envelope *envelope) {
  return envelope->has_header && envelope->header.protocol_version != 0;
}

bool boomlink_encode_envelope(const boomlink_Envelope *envelope, uint8_t *buf, size_t buf_size,
                               size_t *out_len) {
  /* `header` is a singular message-type field, so proto3/Nanopb track its
     presence explicitly (has_header) instead of always encoding it - unlike
     a scalar field, populating envelope.header's members alone does not
     imply presence. Encoding one without has_header set would silently
     produce a wire-valid Envelope with NO header at all rather than the
     caller's intended one (protocol_version/request_id decode as 0 on the
     other end, indistinguishable from a genuine "unused" header) - reject
     it here instead of letting every call site remember to check. Rejecting
     protocol_version == 0 the same way catches the same mistake one level
     down: has_header set (e.g. by assigning header.request_id) but
     protocol_version itself left at its zero default. */
  if (!has_valid_header(envelope)) {
    return false;
  }
  pb_ostream_t stream = pb_ostream_from_buffer(buf, buf_size);
  if (!pb_encode(&stream, boomlink_Envelope_fields, envelope)) {
    return false;
  }
  *out_len = stream.bytes_written;
  return true;
}

bool boomlink_decode_envelope(const uint8_t *buf, size_t len, boomlink_Envelope *out_envelope) {
  *out_envelope = (boomlink_Envelope)boomlink_Envelope_init_zero;
  pb_istream_t stream = pb_istream_from_buffer(buf, len);
  /* pb_decode() does not roll back fields it already wrote before hitting a
     malformed/truncated tag partway through the message - on a false return
     `*out_envelope` can hold a partially-decoded value, not the all-zero one
     the header promises callers on failure. Re-zero on every failure path
     below rather than relying on pb_decode's partial state being harmless by
     accident. */
  if (!pb_decode(&stream, boomlink_Envelope_fields, out_envelope)) {
    *out_envelope = (boomlink_Envelope)boomlink_Envelope_init_zero;
    return false;
  }
  /* A wire-valid Envelope with no header (or protocol_version == 0 - see
     has_valid_header) is malformed at the application level: boomlink.md
     section 7 has every message carry one, starting at version 1. Treat it
     the same as any other decode failure rather than handing the caller a
     "successfully decoded" value with a meaningless header. */
  if (!has_valid_header(out_envelope)) {
    *out_envelope = (boomlink_Envelope)boomlink_Envelope_init_zero;
    return false;
  }
  return true;
}
