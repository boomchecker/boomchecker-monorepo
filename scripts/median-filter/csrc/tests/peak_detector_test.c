#include "peak_detector.h"
#include "unity.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_state_size_and_init(void) {
  struct median_detector_cfg cfg = {
      .num_taps = 3,
      .tap_size = 2,
      .levels = {.det_level = 10, .det_rms = 5, .det_energy = 2},
  };

  size_t need = 0;
  enum peak_det_state st = detector_state_size(&cfg, &need);
  TEST_ASSERT_EQUAL(PEAK_DET_OK, st);
  TEST_ASSERT_TRUE(need > 0);

  uint8_t *buf = (uint8_t *)malloc(need);
  TEST_ASSERT_NOT_NULL(buf);
  struct detector_state *state = NULL;
  st = detector_init(buf, need, &cfg, &state);
  TEST_ASSERT_EQUAL(PEAK_DET_OK, st);
  TEST_ASSERT_NOT_NULL(state);

  detector_reset(state);
  detector_deinit(state);
  free(buf);
}

static void test_buffer_too_small(void) {
  struct median_detector_cfg cfg = {
      .num_taps = 3,
      .tap_size = 2,
      .levels = {.det_level = 0, .det_rms = 0, .det_energy = 0},
  };

  size_t need = 0;
  enum peak_det_state st = detector_state_size(&cfg, &need);
  TEST_ASSERT_EQUAL(PEAK_DET_OK, st);

  uint8_t *buf = (uint8_t *)malloc(need);
  TEST_ASSERT_NOT_NULL(buf);
  struct detector_state *state = NULL;

  st = detector_init(buf, need - 1, &cfg, &state);
  TEST_ASSERT_EQUAL(PEAK_DET_ERR_BUFFER_TOO_SMALL, st);

  free(buf);
}

static void test_invalid_config(void) {
  struct median_detector_cfg cfg_zero_tap = {
      .num_taps = 0,
      .tap_size = 2,
      .levels = {.det_level = 0, .det_rms = 0, .det_energy = 0},
  };
  size_t need = 0;
  enum peak_det_state st = detector_state_size(&cfg_zero_tap, &need);
  TEST_ASSERT_EQUAL(PEAK_DET_ERR_CFG_UNINITIALIZED, st);

  st = detector_state_size(NULL, &need);
  TEST_ASSERT_EQUAL(PEAK_DET_ERR_CFG_UNINITIALIZED, st);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_state_size_and_init);
  RUN_TEST(test_buffer_too_small);
  RUN_TEST(test_invalid_config);
  return UNITY_END();
}
