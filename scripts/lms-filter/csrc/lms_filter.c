#include "lms_filter.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
  FXLMS_SECONDARY_TAPS = 4,
  FXLMS_SECONDARY_ESTIMATE_TAPS = 4,
};

#ifdef FXLMS_TESTING
#define FXLMS_PATH_CONST
#else
#define FXLMS_PATH_CONST const
#endif

/* Q15 FIR taps, newest-sample-first. */
static FXLMS_PATH_CONST int16_t k_secondary_path[FXLMS_SECONDARY_TAPS] = {
    20316,  /*  0.62 */
    -5898,  /* -0.18 */
    3932,   /*  0.12 */
    1638,   /*  0.05 */
};

static FXLMS_PATH_CONST int16_t k_secondary_path_estimate[FXLMS_SECONDARY_ESTIMATE_TAPS] = {
    18022,  /*  0.55 */
    -4915,  /* -0.15 */
    2949,   /*  0.09 */
    1311,   /*  0.04 */
};

struct fxlms_state {
  struct fxlms_config cfg;
  uint16_t pos;
  uint16_t secondary_pos;
  uint16_t estimate_pos;
  int16_t *weights_g;
  int16_t *reference_history;
  int16_t *filtered_x_history;
  int16_t *secondary_history;
  int16_t *estimate_history;
};

static size_t align_up(size_t value, size_t alignment) {
  return (value + alignment - 1U) & ~(alignment - 1U);
}

static int16_t sat_i16(int32_t value, int16_t limit) {
  if (value > limit) {
    return limit;
  }
  if (value < -(int32_t)limit) {
    return (int16_t)(-limit);
  }
  return (int16_t)value;
}

static bool valid_config(const struct fxlms_config *cfg) {
  return cfg != NULL && cfg->taps > 0U && cfg->taps <= 512U &&
         cfg->mu_q15 >= 0 && cfg->adapt_shift <= 30U &&
         cfg->output_limit > 0;
}

static int16_t fir_q15(const int16_t *coeffs, const int16_t *history,
                       uint16_t newest_pos, uint16_t history_len,
                       uint16_t taps, int16_t limit) {
  int64_t acc = 0;
  uint16_t hist_idx = newest_pos;
  for (uint16_t i = 0; i < taps; ++i) {
    acc += (int32_t)coeffs[i] * (int32_t)history[hist_idx];
    hist_idx = (hist_idx == 0U) ? (uint16_t)(history_len - 1U)
                                : (uint16_t)(hist_idx - 1U);
  }
  return sat_i16((int32_t)(acc >> 15), limit);
}

static int32_t shift_trunc_i64(int64_t value, uint8_t shift) {
  if (value >= 0) {
    return (int32_t)(value >> shift);
  }
  return -(int32_t)((-value) >> shift);
}

struct fxlms_config fxlms_default_config(void) {
  struct fxlms_config cfg;
  cfg.taps = (uint16_t)CONFIG_FXLMS_TAPS;
  cfg.mu_q15 = (int16_t)CONFIG_FXLMS_MU_Q15;
  cfg.adapt_shift = (uint8_t)CONFIG_FXLMS_ADAPT_SHIFT;
  cfg.output_limit = (int16_t)CONFIG_FXLMS_OUTPUT_LIMIT;
  return cfg;
}

int fxlms_state_size(const struct fxlms_config *cfg, size_t *out_size) {
  if (out_size == NULL) {
    return FXLMS_ERR_INVALID_ARG;
  }
  if (!valid_config(cfg)) {
    return FXLMS_ERR_INVALID_CONFIG;
  }

  size_t offset = align_up(sizeof(struct fxlms_state), _Alignof(int16_t));
  offset += (size_t)cfg->taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  offset += (size_t)cfg->taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  offset += (size_t)cfg->taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  offset += FXLMS_SECONDARY_TAPS * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  offset += FXLMS_SECONDARY_ESTIMATE_TAPS * sizeof(int16_t);
  *out_size = offset;
  return FXLMS_OK;
}

int fxlms_init(void *mem, size_t mem_size, const struct fxlms_config *cfg,
               struct fxlms_state **out) {
  if (mem == NULL || out == NULL) {
    return FXLMS_ERR_INVALID_ARG;
  }
  size_t needed = 0;
  int st = fxlms_state_size(cfg, &needed);
  if (st != FXLMS_OK) {
    return st;
  }
  if (mem_size < needed) {
    return FXLMS_ERR_BUFFER_TOO_SMALL;
  }

  memset(mem, 0, needed);
  struct fxlms_state *state = (struct fxlms_state *)mem;
  size_t offset = align_up(sizeof(struct fxlms_state), _Alignof(int16_t));
  state->weights_g = (int16_t *)((uint8_t *)mem + offset);
  offset += (size_t)cfg->taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  state->reference_history = (int16_t *)((uint8_t *)mem + offset);
  offset += (size_t)cfg->taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  state->filtered_x_history = (int16_t *)((uint8_t *)mem + offset);
  offset += (size_t)cfg->taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  state->secondary_history = (int16_t *)((uint8_t *)mem + offset);
  offset += FXLMS_SECONDARY_TAPS * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  state->estimate_history = (int16_t *)((uint8_t *)mem + offset);

  state->cfg = *cfg;
  *out = state;
  return FXLMS_OK;
}

int fxlms_reset(struct fxlms_state *state) {
  if (state == NULL) {
    return FXLMS_ERR_INVALID_ARG;
  }
  memset(state->weights_g, 0, (size_t)state->cfg.taps * sizeof(int16_t));
  memset(state->reference_history, 0, (size_t)state->cfg.taps * sizeof(int16_t));
  memset(state->filtered_x_history, 0, (size_t)state->cfg.taps * sizeof(int16_t));
  memset(state->secondary_history, 0, FXLMS_SECONDARY_TAPS * sizeof(int16_t));
  memset(state->estimate_history, 0,
         FXLMS_SECONDARY_ESTIMATE_TAPS * sizeof(int16_t));
  state->pos = 0;
  state->secondary_pos = 0;
  state->estimate_pos = 0;
  return FXLMS_OK;
}

int fxlms_process_sample(struct fxlms_state *state, int16_t reference_x,
                         int16_t primary_d, struct fxlms_sample_result *out) {
  if (state == NULL || out == NULL) {
    return FXLMS_ERR_INVALID_ARG;
  }

  const uint16_t taps = state->cfg.taps;
  state->reference_history[state->pos] = reference_x;

  int16_t controller_y =
      fir_q15(state->weights_g, state->reference_history, state->pos, taps,
              taps, state->cfg.output_limit);

  state->secondary_history[state->secondary_pos] = controller_y;
  int16_t secondary_output =
      fir_q15(k_secondary_path, state->secondary_history, state->secondary_pos,
              FXLMS_SECONDARY_TAPS, FXLMS_SECONDARY_TAPS,
              state->cfg.output_limit);

  int16_t error =
      sat_i16((int32_t)primary_d - (int32_t)secondary_output,
              state->cfg.output_limit);

  state->estimate_history[state->estimate_pos] = reference_x;
  int16_t filtered_x =
      fir_q15(k_secondary_path_estimate, state->estimate_history,
              state->estimate_pos, FXLMS_SECONDARY_ESTIMATE_TAPS,
              FXLMS_SECONDARY_ESTIMATE_TAPS, state->cfg.output_limit);
  state->filtered_x_history[state->pos] = filtered_x;

  uint16_t hist_idx = state->pos;
  for (uint16_t i = 0; i < taps; ++i) {
    int64_t delta = (int64_t)state->cfg.mu_q15 * (int64_t)error *
                    (int64_t)state->filtered_x_history[hist_idx];
    int32_t updated = (int32_t)state->weights_g[i] +
                      shift_trunc_i64(delta,
                                      (uint8_t)(30U + state->cfg.adapt_shift));
    state->weights_g[i] = sat_i16(updated, 32767);
    hist_idx = (hist_idx == 0U) ? (uint16_t)(taps - 1U)
                                : (uint16_t)(hist_idx - 1U);
  }

  state->pos = (uint16_t)((state->pos + 1U) % taps);
  state->secondary_pos =
      (uint16_t)((state->secondary_pos + 1U) % FXLMS_SECONDARY_TAPS);
  state->estimate_pos =
      (uint16_t)((state->estimate_pos + 1U) % FXLMS_SECONDARY_ESTIMATE_TAPS);

  out->controller_y = controller_y;
  out->secondary_output = secondary_output;
  out->error = error;
  return FXLMS_OK;
}

int fxlms_process_block(struct fxlms_state *state, const int16_t *reference_x,
                        const int16_t *primary_d, int16_t *error_e,
                        int16_t *controller_y, int16_t *secondary_output,
                        size_t n) {
  if (state == NULL || reference_x == NULL || primary_d == NULL ||
      error_e == NULL) {
    return FXLMS_ERR_INVALID_ARG;
  }

  for (size_t i = 0; i < n; ++i) {
    struct fxlms_sample_result res;
    int st = fxlms_process_sample(state, reference_x[i], primary_d[i], &res);
    if (st != FXLMS_OK) {
      return st;
    }
    error_e[i] = res.error;
    if (controller_y != NULL) {
      controller_y[i] = res.controller_y;
    }
    if (secondary_output != NULL) {
      secondary_output[i] = res.secondary_output;
    }
  }
  return FXLMS_OK;
}

#ifdef FXLMS_TESTING
int fxlms_test_set_controller_tap(struct fxlms_state *state, uint16_t tap,
                                  int16_t value) {
  if (state == NULL || tap >= state->cfg.taps) {
    return FXLMS_ERR_INVALID_ARG;
  }
  state->weights_g[tap] = value;
  return FXLMS_OK;
}

int fxlms_test_set_secondary_path(const int16_t *coeffs, size_t n) {
  if (coeffs == NULL || n != FXLMS_SECONDARY_TAPS) {
    return FXLMS_ERR_INVALID_ARG;
  }
  memcpy(k_secondary_path, coeffs, sizeof(k_secondary_path));
  return FXLMS_OK;
}

int fxlms_test_set_secondary_estimate(const int16_t *coeffs, size_t n) {
  if (coeffs == NULL || n != FXLMS_SECONDARY_ESTIMATE_TAPS) {
    return FXLMS_ERR_INVALID_ARG;
  }
  memcpy(k_secondary_path_estimate, coeffs, sizeof(k_secondary_path_estimate));
  return FXLMS_OK;
}
#endif
