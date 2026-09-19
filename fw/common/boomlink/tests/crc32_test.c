/**
 ******************************************************************************
 * @file    crc32_test.c
 * @brief   Tests for boomlink_crc32() (storage/boomlink_crc32.h).
 *
 *          The one guarantee that matters: this is the STANDARD CRC-32, not
 *          a lookalike that happens to also produce 32 bits. An invented
 *          checksum can only ever be checked against itself; this one can be
 *          checked against the algorithm's own published test vector.
 ******************************************************************************
 */
#include "boomlink_crc32.h"

#include <string.h>

#include "c_test.h"

BOOMLINK_TEST_STATE;

static void test_matches_the_published_reference_vector(void) {
  const uint8_t vector[] = "123456789";
  /* The canonical CRC-32/ISO-HDLC (== IEEE 802.3/zlib/PNG) test vector: every
     implementation of this exact variant agrees on this one answer. strlen,
     not sizeof, to exclude the string literal's trailing NUL - the vector is
     defined over the 9 ASCII digits, not 10 bytes. */
  CHECK(boomlink_crc32(vector, strlen((const char *)vector)) == 0xCBF43926u,
        "CRC32(\"123456789\") must equal the published reference value 0xCBF43926, got 0x%08X",
        (unsigned)boomlink_crc32(vector, strlen((const char *)vector)));
}

static void test_empty_input_is_the_algorithms_own_identity_value(void) {
  CHECK(boomlink_crc32(NULL, 0u) == 0u,
        "CRC32 of zero bytes must be 0, the reference implementation's own CRC32(\"\")");
}

static void test_same_input_is_deterministic(void) {
  const uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  uint32_t      a      = boomlink_crc32(data, sizeof(data));
  uint32_t      b      = boomlink_crc32(data, sizeof(data));
  CHECK(a == b, "the same bytes must always produce the same CRC");
}

static void test_a_single_flipped_bit_changes_the_result(void) {
  /* Not a formal Hamming-distance proof - just the cheapest possible check
     that this is not an accidental constant function or an off-by-nothing
     no-op. */
  uint8_t  data[]  = {0x00, 0x00, 0x00, 0x00};
  uint32_t before  = boomlink_crc32(data, sizeof(data));
  data[0]          = 0x01;
  uint32_t after   = boomlink_crc32(data, sizeof(data));
  CHECK(before != after, "flipping one bit of the input must change the CRC");
}

static void test_length_matters_not_just_content(void) {
  /* boomlink_config_store_load() CRCs only the DECLARED protobuf_length of
     the stored blob, not the whole buffer - so appending trailing garbage
     (e.g. bytes left over from a shorter previous save) must be detectable,
     not silently absorbed into the same checksum as the true prefix. */
  const uint8_t data[]     = {0xAA, 0xBB, 0xCC, 0xDD};
  uint32_t      full       = boomlink_crc32(data, sizeof(data));
  uint32_t      prefix     = boomlink_crc32(data, sizeof(data) - 1u);
  CHECK(full != prefix, "CRC of a buffer must differ from the CRC of a proper prefix of it");
}

int main(void) {
  test_matches_the_published_reference_vector();
  test_empty_input_is_the_algorithms_own_identity_value();
  test_same_input_is_deterministic();
  test_a_single_flipped_bit_changes_the_result();
  test_length_matters_not_just_content();
  BOOMLINK_TEST_REPORT("crc32_test", 5);
}
