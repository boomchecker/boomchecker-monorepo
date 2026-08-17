/**
 ******************************************************************************
 * @file    boomlink_codec.h
 * @brief   Envelope encode/decode wrapper around Nanopb. Target-agnostic: no
 *          STM32/HAL dependency, usable identically from the firmware and
 *          from host-native tests/tools built against the same source.
 ******************************************************************************
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "envelope.pb.h"

/* header.proto: "protocol_version: BoomProtocol compatibility version,
   initially 1". The one place that number is spelled out - callers should
   use this macro (or boomlink_envelope_init(), below) instead of a bare `1`
   so a future protocol bump is a one-line change. */
#define BOOMLINK_PROTOCOL_VERSION 1u

/* BoomLink's fixed binary link frame header (boomlink.md section 7.3) is not
   implemented until PR 3, but its size is fixed at 20 bytes and is needed
   here to compute the real on-air budget for an Envelope - see
   BOOMLINK_ENVELOPE_BUDGET in boomlink_codec.c. Exposed publicly so firmware
   code that also sees the real RADIO_MAX_PAYLOAD (fw/bom-stm32node/App/radio/
   radio.h) can cross-check the two against each other at compile time,
   instead of boomlink_codec.c's own budget check being the only thing
   guarding a number it has to assume rather than verify. */
#define BOOMLINK_LINK_FRAME_HEADER_SIZE 20

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize `*envelope` to a valid, empty Envelope: header present,
 * protocol_version = BOOMLINK_PROTOCOL_VERSION, request_id = 0, no payload
 * selected. Callers then set request_id and the payload oneof themselves.
 * Using this instead of hand-assigning `has_header`/`protocol_version`
 * removes the two footguns boomlink_encode_envelope() otherwise has to
 * reject at encode time (see its doc comment).
 */
void boomlink_envelope_init(boomlink_Envelope *envelope);

/**
 * Serialize `envelope` into `buf` (capacity `buf_size`). On success writes
 * the encoded length to `*out_len` and returns true. Returns false without
 * modifying `*out_len` if `envelope` is malformed - no header, protocol_version
 * left at its unset value of 0 (see boomlink_envelope_init()), or a bounded
 * field left longer than its `.options` max_size - or if `buf` is too small
 * to hold it. A buffer sized `boomlink_Envelope_size` (see envelope.pb.h)
 * always fits any value that satisfies the schema's bounds.
 */
bool boomlink_encode_envelope(const boomlink_Envelope *envelope, uint8_t *buf, size_t buf_size,
                               size_t *out_len);

/**
 * Deserialize `len` bytes at `buf` into `*out_envelope`. Returns false on a
 * malformed or truncated encoding, or one missing a valid header (see
 * boomlink_encode_envelope()); `*out_envelope` is left as
 * `boomlink_Envelope_init_zero` in that case rather than a partial value.
 *
 * A successful decode can still carry no application payload at all
 * (`out_envelope->which_payload == 0`) - an Envelope with only a header is
 * wire-valid - or a payload this build's schema doesn't know about yet (a
 * peer running a newer BoomProtocol version - Nanopb silently skips unknown
 * oneof branches rather than erroring). Callers that dispatch on
 * `which_payload` must handle both.
 */
bool boomlink_decode_envelope(const uint8_t *buf, size_t len, boomlink_Envelope *out_envelope);

#ifdef __cplusplus
}
#endif
