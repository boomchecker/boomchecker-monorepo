/**
 ******************************************************************************
 * @file    boomlink_crc32.h
 * @brief   The CRC-32 boomlink_config_store.h's wrapper format uses to detect
 *          a corrupt or torn flash write (boomlink.md section 10.1).
 *
 *          Pulled out of boomlink_config_store.c into its own tiny,
 *          Nanopb-free unit specifically so it can be tested against the
 *          standard's own published vector directly (crc32_test.c), the same
 *          reasoning that gives boomlink_dupcache and boomlink_txqueue their
 *          own dedicated tests even though both are built into the larger
 *          link engine library - a small self-contained algorithm is worth
 *          pinning on its own, not only indirectly through whatever happens
 *          to call it.
 ******************************************************************************
 */
#ifndef BOOMLINK_CRC32_H
#define BOOMLINK_CRC32_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The standard CRC-32 (IEEE 802.3/zlib/PNG variant - reflected, poly
 * 0xEDB88320, init 0xFFFFFFFF, final XOR 0xFFFFFFFF), not a bespoke
 * checksum: its correctness can be checked against a published test vector
 * (boomlink_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926 -
 * crc32_test.c exercises exactly this) instead of trusting an invented one.
 *
 * Bit-by-bit rather than table-based: every current caller's input is a
 * NodeConfig blob under 200 bytes, so a 1KB lookup table would cost more
 * flash than a value this small could ever save.
 *
 * @param data start of the buffer to checksum. May be NULL only if `len` is
 *             0.
 * @param len  number of bytes at `data` to checksum.
 * @return the CRC-32 of the `len` bytes at `data` (0u for a zero-length
 *         input, same as the reference implementation's CRC32("")).
 */
uint32_t boomlink_crc32(const uint8_t *data, size_t len);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* BOOMLINK_CRC32_H */
