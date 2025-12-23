#include "peak_detector.h"
#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void generate_signal(int16_t *dst, size_t num_taps, size_t tap_size,
                            size_t peak_tap_idx, size_t peak_pos,
                            int16_t peak_val) {
  memset(dst, 0, num_taps * tap_size * sizeof(int16_t));
  if (peak_tap_idx < num_taps && peak_pos < tap_size) {
    dst[peak_tap_idx * tap_size + peak_pos] = peak_val;
  }
}

static void test_detect_recording_basic(void) {
  // num_taps=3, tap_size=2; peak je v prostředním tapu po naplnění okna
  struct median_detector_cfg cfg = {
      .num_taps = 3,
      .tap_size = 2,
      .levels = {.det_level = 1, .det_rms = 0, .det_energy = 0},
  };

  int16_t samples[6] = {
      0, 0,  // tap0
      10, 0, // tap1 s peakem na pozici 0
      0, 0,  // tap2
  };

  int positions[4] = {-1, -1, -1, -1};
  int hits = detect_recording_i16(samples, 6, &cfg, positions, 4);

  TEST_ASSERT_EQUAL(1, hits);
  TEST_ASSERT_EQUAL(2, positions[0]); // peak_index = začátek middle tapu (offset 2)
  TEST_ASSERT_EQUAL(-1, positions[1]);
}

static void test_detect_recording_multiple_hits(void) {
  // num_taps=3, tap_size=4; dva peaky v postupných middle tap posunech
  struct median_detector_cfg cfg = {
      .num_taps = 3,
      .tap_size = 4,
      .levels = {.det_level = 2, .det_rms = 0, .det_energy = 0},
  };

  int16_t samples[5 * 4];
  memset(samples, 0, sizeof(samples));
  // peak v tap1 pos1
  samples[1 * 4 + 1] = 5;
  samples[1 * 4 + 2] = 1;
  samples[1 * 4 + 3] = 1;
  // peak v tap2 pos2
  samples[2 * 4 + 2] = 6;
  samples[2 * 4 + 3] = 1;

  int positions[4] = {-1, -1, -1, -1};
  int hits = detect_recording_i16(samples, 20, &cfg, positions, 4);

  // Middle tap se vyhodnocuje po naplnění okna; očekáváme 2 zásahy (tap1, pak tap2)
  TEST_ASSERT_EQUAL(2, hits);
  TEST_ASSERT_EQUAL(1 * cfg.tap_size + 1, positions[0]);
  TEST_ASSERT_EQUAL(2 * cfg.tap_size + 2, positions[1]);
}

static void test_detect_recording_large_generated(void) {
  // větší pole: num_taps=4, tap_size=6; dva peaky
  struct median_detector_cfg cfg = {
      .num_taps = 4,
      .tap_size = 6,
      .levels = {.det_level = 3, .det_rms = 0, .det_energy = 0},
  };

  const size_t total_taps = 7; // víc než num_taps kvůli posunu okna
  int16_t *samples =
      (int16_t *)malloc(total_taps * cfg.tap_size * sizeof(int16_t));
  TEST_ASSERT_NOT_NULL(samples);

  memset(samples, 0, total_taps * cfg.tap_size * sizeof(int16_t));
  // peak1 v tap2 pos4
  samples[2 * cfg.tap_size + 4] = 9;
  samples[2 * cfg.tap_size + 5] = 2;
  // peak2 v tap3 pos3
  samples[3 * cfg.tap_size + 3] = 7;
  samples[3 * cfg.tap_size + 4] = 2;
  samples[3 * cfg.tap_size + 5] = 2;

  int positions[8] = {-1};
  int hits = detect_recording_i16(samples, total_taps * cfg.tap_size, &cfg,
                                  positions, 8);

  TEST_ASSERT_EQUAL(2, hits);
  TEST_ASSERT_EQUAL((int)(2 * cfg.tap_size + 4), positions[0]);
  TEST_ASSERT_EQUAL((int)(3 * cfg.tap_size + 3), positions[1]);

  free(samples);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_detect_recording_basic);
  RUN_TEST(test_detect_recording_multiple_hits);
  RUN_TEST(test_detect_recording_large_generated);
  return UNITY_END();
}
