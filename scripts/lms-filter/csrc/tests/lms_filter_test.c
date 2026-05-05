#include "lms_filter.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ASSERT_TRUE(expr)                                                        \
  do {                                                                          \
    if (!(expr)) {                                                              \
      fprintf(stderr, "ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__,  \
              #expr);                                                           \
      exit(1);                                                                  \
    }                                                                           \
  } while (0)

#define ASSERT_EQ_INT(expected, actual)                                          \
  do {                                                                          \
    int exp_ = (expected);                                                       \
    int act_ = (actual);                                                         \
    if (exp_ != act_) {                                                          \
      fprintf(stderr, "ASSERT_EQ_INT failed at %s:%d: expected %d got %d\n",     \
              __FILE__, __LINE__, exp_, act_);                                  \
      exit(1);                                                                  \
    }                                                                           \
  } while (0)

static void test_state_size_rejects_bad_config(void) {
  struct lms_config cfg = lms_default_config();
  size_t size = 0;
  ASSERT_EQ_INT(LMS_OK, lms_state_size(&cfg, &size));
  ASSERT_TRUE(size > sizeof(struct lms_config));

  cfg.taps = 0;
  ASSERT_EQ_INT(LMS_ERR_INVALID_CONFIG, lms_state_size(&cfg, &size));
}

static void test_reset_clears_history(void) {
  struct lms_config cfg = {
      .taps = 4, .mu_q15 = 128, .adapt_shift = 0, .output_limit = 32767};
  size_t size = 0;
  ASSERT_EQ_INT(LMS_OK, lms_state_size(&cfg, &size));
  uint8_t *mem = calloc(1, size);
  ASSERT_TRUE(mem != NULL);

  struct lms_state *state = NULL;
  ASSERT_EQ_INT(LMS_OK, lms_init(mem, size, &cfg, &state));
  struct lms_sample_result res;
  ASSERT_EQ_INT(LMS_OK, lms_process_sample(state, 12000, 6000, &res));
  ASSERT_EQ_INT(LMS_OK, lms_reset(state));
  ASSERT_EQ_INT(LMS_OK, lms_process_sample(state, 0, 0, &res));
  ASSERT_EQ_INT(0, res.estimated_noise);
  ASSERT_EQ_INT(0, res.error);

  free(mem);
}

static void test_converges_on_single_tap_path(void) {
  struct lms_config cfg = {
      .taps = 1, .mu_q15 = 256, .adapt_shift = 0, .output_limit = 32767};
  size_t size = 0;
  ASSERT_EQ_INT(LMS_OK, lms_state_size(&cfg, &size));
  uint8_t *mem = calloc(1, size);
  ASSERT_TRUE(mem != NULL);

  struct lms_state *state = NULL;
  ASSERT_EQ_INT(LMS_OK, lms_init(mem, size, &cfg, &state));

  int64_t abs_err_first = 0;
  int64_t abs_err_last = 0;
  for (int i = 0; i < 3000; ++i) {
    int16_t x = (i & 1) ? 12000 : -12000;
    int16_t d = (int16_t)(((int32_t)x * 16384) >> 15);
    struct lms_sample_result res;
    ASSERT_EQ_INT(LMS_OK, lms_process_sample(state, x, d, &res));
    int32_t abs_err = res.error < 0 ? -(int32_t)res.error : res.error;
    if (i < 100) {
      abs_err_first += abs_err;
    }
    if (i >= 2900) {
      abs_err_last += abs_err;
    }
  }
  ASSERT_TRUE(abs_err_last < abs_err_first / 4);

  free(mem);
}

static void test_block_api_outputs_error(void) {
  struct lms_config cfg = {
      .taps = 2, .mu_q15 = 0, .adapt_shift = 0, .output_limit = 32767};
  size_t size = 0;
  ASSERT_EQ_INT(LMS_OK, lms_state_size(&cfg, &size));
  uint8_t *mem = calloc(1, size);
  ASSERT_TRUE(mem != NULL);

  struct lms_state *state = NULL;
  ASSERT_EQ_INT(LMS_OK, lms_init(mem, size, &cfg, &state));

  const int16_t ref[3] = {100, 200, 300};
  const int16_t desired[3] = {1000, -2000, 3000};
  int16_t cleaned[3] = {0};
  ASSERT_EQ_INT(LMS_OK, lms_process_block(state, ref, desired, cleaned, NULL, 3));
  ASSERT_EQ_INT(1000, cleaned[0]);
  ASSERT_EQ_INT(-2000, cleaned[1]);
  ASSERT_EQ_INT(3000, cleaned[2]);

  free(mem);
}

int main(void) {
  test_state_size_rejects_bad_config();
  test_reset_clears_history();
  test_converges_on_single_tap_path();
  test_block_api_outputs_error();
  puts("lms_filter_tests: ok");
  return 0;
}
