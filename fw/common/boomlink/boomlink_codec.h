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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Serialize `envelope` into `buf` (capacity `buf_size`). On success writes
 * the encoded length to `*out_len` and returns true. Returns false without
 * modifying `*out_len` if `envelope` is malformed (e.g. a bounded field left
 * longer than its `.options` max_size) or `buf` is too small to hold it -
 * a buffer sized `boomlink_Envelope_size` (see envelope.pb.h) always fits any
 * value that satisfies the schema's bounds.
 */
bool boomlink_encode_envelope(const boomlink_Envelope *envelope, uint8_t *buf, size_t buf_size,
                               size_t *out_len);

/**
 * Deserialize `len` bytes at `buf` into `*out_envelope`. Returns false on a
 * malformed or truncated encoding; `*out_envelope` is left as
 * `boomlink_Envelope_init_zero` in that case rather than a partial value.
 */
bool boomlink_decode_envelope(const uint8_t *buf, size_t len, boomlink_Envelope *out_envelope);

#ifdef __cplusplus
}
#endif
