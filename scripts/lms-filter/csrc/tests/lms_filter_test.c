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

  cfg = fxlms_default_config();
  cfg.reference_count = 5;
  ASSERT_EQ_INT(FXLMS_ERR_INVALID_CONFIG, fxlms_state_size(&cfg, &size));

  cfg = fxlms_default_config();
  cfg.actuator_count = 5;
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

  const int16_t mu = 1000;
  struct fxlms_config cfg = {
      .taps = 1, .mu_q15 = mu, .adapt_shift = 0, .output_limit = 32767};
  uint8_t *mem = NULL;
  struct fxlms_state *state = make_state(&cfg, &mem);

  struct fxlms_sample_result res;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state, 20000, 20000, &res));
  ASSERT_EQ_INT(0, res.secondary_output);

  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state, 20000, 0, &res));
  /* Normalized step: g += mu * e * x_f / (sum x_f^2 + eps). With taps=1 and a
   * single channel the window holds exactly one filtered-x sample. */
  int16_t filtered_x = q15_mul(16384, 20000);
  int64_t power = (int64_t)filtered_x * filtered_x;
  int64_t eps = (int64_t)1 * 1 * CONFIG_FXLMS_NLMS_POWER_FLOOR;
  int16_t expected_weight =
      (int16_t)(((int64_t)mu * 20000 * filtered_x) / (power + eps));
  int16_t expected_y = q15_mul(expected_weight, 20000);
  ASSERT_EQ_INT(expected_y, res.controller_y);
  ASSERT_EQ_INT(q15_mul(32767, expected_y), res.secondary_output);
  free(mem);
}

static void test_secondary_estimate_changes_only_adaptation_branch(void) {
  const int16_t secondary[4] = {32767, 0, 0, 0};
  const int16_t estimate_a[4] = {8192, 0, 0, 0};
  const int16_t estimate_b[4] = {24576, 0, 0, 0};

  /* Small mu so the normalized single-tap step does not saturate the weight. */
  struct fxlms_config cfg = {
      .taps = 1, .mu_q15 = 200, .adapt_shift = 0, .output_limit = 32767};

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

  /* The estimate only feeds the adaptation branch, so the forward path (output,
   * secondary, error) of the first sample is identical for both estimates. */
  ASSERT_EQ_INT(first_a.controller_y, first_b.controller_y);
  ASSERT_EQ_INT(first_a.secondary_output, first_b.secondary_output);
  ASSERT_EQ_INT(first_a.error, first_b.error);

  struct fxlms_sample_result second_a;
  struct fxlms_sample_result second_b;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state_a, 18000, 0, &second_a));
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_sample(state_b, 18000, 0, &second_b));
  /* The estimate still changes the adapted weights, but with the normalized
   * step the update scales with x_f / (x_f^2 + eps): the smaller estimate (a)
   * produces the smaller filtered-x power and therefore the larger step. */
  ASSERT_TRUE(second_a.controller_y > second_b.controller_y);
  ASSERT_TRUE(second_a.secondary_output > second_b.secondary_output);
  ASSERT_TRUE(second_b.controller_y > 0);

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

static void test_multi_channel_sums_actuator_outputs(void) {
  const int16_t secondary_a[4] = {32767, 0, 0, 0};
  const int16_t secondary_b[4] = {16384, 0, 0, 0};
  const int16_t estimate[4] = {32767, 0, 0, 0};
  ASSERT_EQ_INT(FXLMS_OK, fxlms_test_set_actuator_secondary_path(0, secondary_a, 4));
  ASSERT_EQ_INT(FXLMS_OK, fxlms_test_set_actuator_secondary_path(1, secondary_b, 4));
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_test_set_actuator_secondary_estimate(0, estimate, 4));
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_test_set_actuator_secondary_estimate(1, estimate, 4));

  struct fxlms_config cfg = {.taps = 1,
                             .reference_count = 2,
                             .actuator_count = 2,
                             .mu_q15 = 0,
                             .adapt_shift = 0,
                             .output_limit = 32767};
  uint8_t *mem = NULL;
  struct fxlms_state *state = make_state(&cfg, &mem);
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_test_set_multi_controller_tap(state, 0, 0, 0, 16384));
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_test_set_multi_controller_tap(state, 0, 1, 0, 8192));
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_test_set_multi_controller_tap(state, 1, 0, 0, 4096));
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_test_set_multi_controller_tap(state, 1, 1, 0, 2048));

  const int16_t refs[2] = {12000, 8000};
  struct fxlms_multi_sample_result res;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_multi_sample(state, refs, 20000, &res));

  int16_t y0 = (int16_t)(q15_mul(16384, refs[0]) + q15_mul(8192, refs[1]));
  int16_t y1 = (int16_t)(q15_mul(4096, refs[0]) + q15_mul(2048, refs[1]));
  int16_t sec0 = q15_mul(32767, y0);
  int16_t sec1 = q15_mul(16384, y1);
  ASSERT_EQ_INT(y0, res.controller_y[0]);
  ASSERT_EQ_INT(y1, res.controller_y[1]);
  ASSERT_EQ_INT(sec0, res.secondary_output[0]);
  ASSERT_EQ_INT(sec1, res.secondary_output[1]);
  ASSERT_EQ_INT((int16_t)(sec0 + sec1), res.secondary_sum);
  ASSERT_EQ_INT(20000 - res.secondary_sum, res.error);
  free(mem);
}

static void test_multi_reference_controller_saturates_after_sum(void) {
  const int16_t secondary[4] = {32767, 0, 0, 0};
  const int16_t estimate[4] = {32767, 0, 0, 0};
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_test_set_actuator_secondary_path(0, secondary, 4));
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_test_set_actuator_secondary_estimate(0, estimate, 4));

  struct fxlms_config cfg = {.taps = 1,
                             .reference_count = 2,
                             .actuator_count = 1,
                             .mu_q15 = 0,
                             .adapt_shift = 0,
                             .output_limit = 10000};
  uint8_t *mem = NULL;
  struct fxlms_state *state = make_state(&cfg, &mem);
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_test_set_multi_controller_tap(state, 0, 0, 0, 32767));
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_test_set_multi_controller_tap(state, 0, 1, 0, -24576));

  const int16_t refs[2] = {30000, 30000};
  struct fxlms_multi_sample_result res;
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_multi_sample(state, refs, 0, &res));

  int expected_y = q15_mul(32767, refs[0]) + q15_mul(-24576, refs[1]);
  ASSERT_TRUE(expected_y > -10000 && expected_y < 10000);
  ASSERT_EQ_INT(expected_y, res.controller_y[0]);
  ASSERT_EQ_INT(q15_mul(32767, (int16_t)expected_y), res.secondary_output[0]);
  ASSERT_EQ_INT(-res.secondary_output[0], res.error);
  free(mem);
}

static void test_multi_block_uses_channel_major_buffers(void) {
  const int16_t secondary[4] = {32767, 0, 0, 0};
  const int16_t estimate[4] = {32767, 0, 0, 0};
  ASSERT_EQ_INT(FXLMS_OK, fxlms_test_set_actuator_secondary_path(0, secondary, 4));
  ASSERT_EQ_INT(FXLMS_OK, fxlms_test_set_actuator_secondary_estimate(0, estimate, 4));

  struct fxlms_config cfg = {.taps = 1,
                             .reference_count = 2,
                             .actuator_count = 1,
                             .mu_q15 = 0,
                             .adapt_shift = 0,
                             .output_limit = 32767};
  uint8_t *mem = NULL;
  struct fxlms_state *state = make_state(&cfg, &mem);
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_test_set_multi_controller_tap(state, 0, 0, 0, 16384));
  ASSERT_EQ_INT(FXLMS_OK,
                fxlms_test_set_multi_controller_tap(state, 0, 1, 0, 8192));

  const int16_t refs[6] = {
      100, 200, 300,
      1000, 2000, 3000,
  };
  const int16_t primary[3] = {0, 0, 0};
  int16_t error[3] = {0};
  int16_t y[3] = {0};
  int16_t secondary_out[3] = {0};
  ASSERT_EQ_INT(FXLMS_OK, fxlms_process_multi_block(state, refs, primary, error,
                                                    y, secondary_out, 3));
  for (int i = 0; i < 3; ++i) {
    int16_t expected_y =
        (int16_t)(q15_mul(16384, refs[i]) + q15_mul(8192, refs[3 + i]));
    ASSERT_EQ_INT(expected_y, y[i]);
    ASSERT_EQ_INT(q15_mul(32767, expected_y), secondary_out[i]);
    ASSERT_EQ_INT(-secondary_out[i], error[i]);
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
  test_multi_channel_sums_actuator_outputs();
  test_multi_reference_controller_saturates_after_sum();
  test_multi_block_uses_channel_major_buffers();
  puts("fxlms_filter_tests: ok");
  return 0;
}
