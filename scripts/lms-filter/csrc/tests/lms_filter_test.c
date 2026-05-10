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

static int16_t q15_mul(int16_t a, int16_t b) {
  return (int16_t)(((int32_t)a * (int32_t)b) >> 15);
}

static struct fxlms_state *make_state(const struct fxlms_config *cfg,
                                      uint8_t **mem_out) {
  size_t size = 0;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_state_size(cfg, &size));
  uint8_t *mem = calloc(1, size);
  ASSERT_TRUE(mem != NULL);

  struct fxlms_state *state = NULL;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_init(mem, size, cfg, &state));
  *mem_out = mem;
  return state;
}

static void set_paths(const int16_t secondary[4], const int16_t estimate[4]) {
  ASSERT_EQ_INT(FXLMS_OK, fxlms_test_set_secondary_path(secondary, 4));
  ASSERT_EQ_INT(FXLMS_OK, fxlms_test_set_secondary_estimate(estimate, 4));
}

static void test_state_size_rejects_bad_config(void) {
  struct fxlms_config cfg = fxlms_default_config();
  size_t size = 0;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_state_size(&cfg, &size));
  ASSERT_TRUE(size > sizeof(struct fxlms_config));

  cfg.taps = 0;
  ASSERT_EQ_INT(FXLMS_ERR_INVALID_CONFIG, fxlms_state_size(&cfg, &size));
}

static void test_error_is_primary_minus_secondary_path_output(void) {
  const int16_t secondary[4] = {16384, 8192, 0, 0};
  const int16_t estimate[4] = {16384, 0, 0, 0};
  set_paths(secondary, estimate);

  struct fxlms_config cfg = {
      .taps = 2, .mu_q15 = 0, .adapt_shift = 0, .output_limit = 32767};
  uint8_t *mem = NULL;
  struct fxlms_state *state = make_state(&cfg, &mem);
  ASSERT_EQ_INT(FXLMS_OK, fxlms_test_set_controller_tap(state, 0, 16384));

  struct fxlms_sample_result res;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state, 20000, 10000, &res));
  ASSERT_EQ_INT(q15_mul(16384, 20000), res.controller_y);
  ASSERT_EQ_INT(q15_mul(16384, res.controller_y), res.secondary_output);
  ASSERT_EQ_INT(10000 - res.secondary_output, res.error);

  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state, 0, 0, &res));
  ASSERT_EQ_INT(q15_mul(8192, q15_mul(16384, 20000)), res.secondary_output);
  ASSERT_EQ_INT(-res.secondary_output, res.error);
  free(mem);
}

static void test_filtered_x_update_uses_secondary_estimate(void) {
  const int16_t secondary[4] = {32767, 0, 0, 0};
  const int16_t estimate[4] = {16384, 0, 0, 0};
  set_paths(secondary, estimate);

  struct fxlms_config cfg = {
      .taps = 1, .mu_q15 = 32767, .adapt_shift = 0, .output_limit = 32767};
  uint8_t *mem = NULL;
  struct fxlms_state *state = make_state(&cfg, &mem);

  struct fxlms_sample_result res;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state, 20000, 20000, &res));
  ASSERT_EQ_INT(0, res.secondary_output);

  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state, 20000, 0, &res));
  int16_t filtered_x = q15_mul(16384, 20000);
  int16_t expected_weight =
      (int16_t)(((int64_t)32767 * 20000 * filtered_x) >> 30);
  int16_t expected_y = q15_mul(expected_weight, 20000);
  ASSERT_EQ_INT(expected_y, res.controller_y);
  ASSERT_EQ_INT(q15_mul(32767, expected_y), res.secondary_output);
  free(mem);
}

static void test_secondary_estimate_changes_only_adaptation_branch(void) {
  const int16_t secondary[4] = {32767, 0, 0, 0};
  const int16_t estimate_a[4] = {8192, 0, 0, 0};
  const int16_t estimate_b[4] = {24576, 0, 0, 0};

  struct fxlms_config cfg = {
      .taps = 1, .mu_q15 = 32767, .adapt_shift = 0, .output_limit = 32767};

  set_paths(secondary, estimate_a);
  uint8_t *mem_a = NULL;
  struct fxlms_state *state_a = make_state(&cfg, &mem_a);
  struct fxlms_sample_result first_a;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state_a, 18000, 16000, &first_a));

  set_paths(secondary, estimate_b);
  uint8_t *mem_b = NULL;
  struct fxlms_state *state_b = make_state(&cfg, &mem_b);
  struct fxlms_sample_result first_b;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state_b, 18000, 16000, &first_b));

  ASSERT_EQ_INT(first_a.controller_y, first_b.controller_y);
  ASSERT_EQ_INT(first_a.secondary_output, first_b.secondary_output);
  ASSERT_EQ_INT(first_a.error, first_b.error);

  struct fxlms_sample_result second_a;
  struct fxlms_sample_result second_b;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state_a, 18000, 0, &second_a));
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state_b, 18000, 0, &second_b));
  ASSERT_TRUE(second_b.controller_y > second_a.controller_y);
  ASSERT_TRUE(second_b.secondary_output > second_a.secondary_output);

  free(mem_a);
  free(mem_b);
}

static void test_newest_sample_first_fir_indexing(void) {
  const int16_t secondary[4] = {32767, 0, 0, 0};
  const int16_t estimate[4] = {32767, 0, 0, 0};
  set_paths(secondary, estimate);

  struct fxlms_config cfg = {
      .taps = 3, .mu_q15 = 0, .adapt_shift = 0, .output_limit = 32767};
  uint8_t *mem = NULL;
  struct fxlms_state *state = make_state(&cfg, &mem);
  ASSERT_EQ_INT(FXLMS_OK, fxlms_test_set_controller_tap(state, 0, 16384));
  ASSERT_EQ_INT(FXLMS_OK, fxlms_test_set_controller_tap(state, 1, 8192));
  ASSERT_EQ_INT(FXLMS_OK, fxlms_test_set_controller_tap(state, 2, 4096));

  struct fxlms_sample_result res;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state, 1000, 0, &res));
  ASSERT_EQ_INT(q15_mul(32767, q15_mul(16384, 1000)), res.secondary_output);

  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state, 2000, 0, &res));
  int16_t expected =
      (int16_t)(q15_mul(16384, 2000) + q15_mul(8192, 1000));
  ASSERT_EQ_INT(q15_mul(32767, expected), res.secondary_output);

  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state, 3000, 0, &res));
  expected = (int16_t)(q15_mul(16384, 3000) + q15_mul(8192, 2000) +
                      q15_mul(4096, 1000));
  ASSERT_EQ_INT(q15_mul(32767, expected), res.secondary_output);
  free(mem);
}

static void test_reset_clears_buffers_and_weights(void) {
  const int16_t secondary[4] = {32767, 8192, 4096, 2048};
  const int16_t estimate[4] = {32767, 0, 0, 0};
  set_paths(secondary, estimate);

  struct fxlms_config cfg = {
      .taps = 2, .mu_q15 = 32767, .adapt_shift = 0, .output_limit = 32767};
  uint8_t *mem = NULL;
  struct fxlms_state *state = make_state(&cfg, &mem);

  struct fxlms_sample_result res;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state, 12000, 18000, &res));
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state, 12000, 0, &res));
  ASSERT_TRUE(res.controller_y != 0 || res.secondary_output != 0);

  ASSERT_EQ_INT(FXLMS_OK, fxlms_reset(state));
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state, 0, 0, &res));
  ASSERT_EQ_INT(0, res.controller_y);
  ASSERT_EQ_INT(0, res.secondary_output);
  ASSERT_EQ_INT(0, res.error);
  free(mem);
}

static void test_block_api_outputs_all_streams(void) {
  const int16_t secondary[4] = {32767, 0, 0, 0};
  const int16_t estimate[4] = {32767, 0, 0, 0};
  set_paths(secondary, estimate);

  struct fxlms_config cfg = {
      .taps = 1, .mu_q15 = 0, .adapt_shift = 0, .output_limit = 32767};
  uint8_t *mem = NULL;
  struct fxlms_state *state = make_state(&cfg, &mem);
  ASSERT_EQ_INT(FXLMS_OK, fxlms_test_set_controller_tap(state, 0, 16384));

  const int16_t ref[3] = {100, 200, 300};
  const int16_t primary[3] = {1000, -2000, 3000};
  int16_t error[3] = {0};
  int16_t y[3] = {0};
  int16_t secondary_out[3] = {0};
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_process_block(state, ref, primary, error, y,
                                    secondary_out, 3));
  for (int i = 0; i < 3; ++i) {
    ASSERT_EQ_INT(q15_mul(16384, ref[i]), y[i]);
    ASSERT_EQ_INT(q15_mul(32767, y[i]), secondary_out[i]);
    ASSERT_EQ_INT(primary[i] - secondary_out[i], error[i]);
  }
  free(mem);
}

int main(void) {
  test_state_size_rejects_bad_config();
  test_error_is_primary_minus_secondary_path_output();
  test_filtered_x_update_uses_secondary_estimate();
  test_secondary_estimate_changes_only_adaptation_branch();
  test_newest_sample_first_fir_indexing();
  test_reset_clears_buffers_and_weights();
  test_block_api_outputs_all_streams();
  puts("fxlms_filter_tests: ok");
  return 0;
}
