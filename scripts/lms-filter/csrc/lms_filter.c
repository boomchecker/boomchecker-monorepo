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
static FXLMS_PATH_CONST int16_t
    k_secondary_path[CONFIG_FXLMS_MAX_ACTUATORS][FXLMS_SECONDARY_TAPS] = {
        {20316, -5898, 3932, 1638},
#if CONFIG_FXLMS_MAX_ACTUATORS >= 2
        {19005, -5243, 3604, 1966},
#endif
#if CONFIG_FXLMS_MAX_ACTUATORS >= 3
        {20971, -6553, 3277, 1311},
#endif
#if CONFIG_FXLMS_MAX_ACTUATORS >= 4
        {18350, -4588, 4259, 983},
#endif
};

static FXLMS_PATH_CONST int16_t
    k_secondary_path_estimate[CONFIG_FXLMS_MAX_ACTUATORS]
                             [FXLMS_SECONDARY_ESTIMATE_TAPS] = {
        {18022, -4915, 2949, 1311},
#if CONFIG_FXLMS_MAX_ACTUATORS >= 2
        {17039, -4424, 2621, 1475},
#endif
#if CONFIG_FXLMS_MAX_ACTUATORS >= 3
        {18678, -5407, 2458, 1147},
#endif
#if CONFIG_FXLMS_MAX_ACTUATORS >= 4
        {16384, -3932, 3277, 819},
#endif
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

static uint8_t normalized_reference_count(const struct fxlms_config *cfg) {
  return (cfg->reference_count == 0U) ? 1U : cfg->reference_count;
}

static uint8_t normalized_actuator_count(const struct fxlms_config *cfg) {
  return (cfg->actuator_count == 0U) ? 1U : cfg->actuator_count;
}

static bool valid_config(const struct fxlms_config *cfg) {
  if (cfg == NULL) {
    return false;
  }
  uint8_t reference_count = normalized_reference_count(cfg);
  uint8_t actuator_count = normalized_actuator_count(cfg);
  return cfg->taps > 0U && cfg->taps <= 512U &&
         reference_count <= CONFIG_FXLMS_MAX_REFERENCES &&
         actuator_count <= CONFIG_FXLMS_MAX_ACTUATORS &&
         cfg->mu_q15 >= 0 && cfg->adapt_shift <= 30U &&
         cfg->output_limit > 0;
}

static int32_t fir_q15_raw(const int16_t *coeffs, const int16_t *history,
                           uint16_t newest_pos, uint16_t history_len,
                           uint16_t taps);

static int16_t fir_q15(const int16_t *coeffs, const int16_t *history,
                       uint16_t newest_pos, uint16_t history_len,
                       uint16_t taps, int16_t limit) {
  int32_t raw =
      fir_q15_raw(coeffs, history, newest_pos, history_len, taps);
  return sat_i16(raw, limit);
}

static int32_t fir_q15_raw(const int16_t *coeffs, const int16_t *history,
                           uint16_t newest_pos, uint16_t history_len,
                           uint16_t taps) {
  int64_t acc = 0;
  uint16_t hist_idx = newest_pos;
  for (uint16_t i = 0; i < taps; ++i) {
    acc += (int32_t)coeffs[i] * (int32_t)history[hist_idx];
    hist_idx = (hist_idx == 0U) ? (uint16_t)(history_len - 1U)
                                : (uint16_t)(hist_idx - 1U);
  }
  return (int32_t)(acc >> 15);
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
  cfg.reference_count = 1;
  cfg.actuator_count = 1;
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

  uint8_t reference_count = normalized_reference_count(cfg);
  uint8_t actuator_count = normalized_actuator_count(cfg);
  size_t controller_count = (size_t)reference_count * actuator_count;

  size_t offset = align_up(sizeof(struct fxlms_state), _Alignof(int16_t));
  offset += controller_count * (size_t)cfg->taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  offset += (size_t)reference_count * cfg->taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  offset += controller_count * (size_t)cfg->taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  offset += (size_t)actuator_count * FXLMS_SECONDARY_TAPS * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  offset +=
      (size_t)reference_count * FXLMS_SECONDARY_ESTIMATE_TAPS * sizeof(int16_t);
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
  struct fxlms_config normalized = *cfg;
  normalized.reference_count = normalized_reference_count(cfg);
  normalized.actuator_count = normalized_actuator_count(cfg);
  size_t controller_count =
      (size_t)normalized.reference_count * normalized.actuator_count;

  size_t offset = align_up(sizeof(struct fxlms_state), _Alignof(int16_t));
  state->weights_g = (int16_t *)((uint8_t *)mem + offset);
  offset += controller_count * (size_t)normalized.taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  state->reference_history = (int16_t *)((uint8_t *)mem + offset);
  offset +=
      (size_t)normalized.reference_count * normalized.taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  state->filtered_x_history = (int16_t *)((uint8_t *)mem + offset);
  offset += controller_count * (size_t)normalized.taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  state->secondary_history = (int16_t *)((uint8_t *)mem + offset);
  offset +=
      (size_t)normalized.actuator_count * FXLMS_SECONDARY_TAPS * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  state->estimate_history = (int16_t *)((uint8_t *)mem + offset);

  state->cfg = normalized;
  *out = state;
  return FXLMS_OK;
}

int fxlms_reset(struct fxlms_state *state) {
  if (state == NULL) {
    return FXLMS_ERR_INVALID_ARG;
  }
  size_t controller_count =
      (size_t)state->cfg.reference_count * state->cfg.actuator_count;
  memset(state->weights_g, 0,
         controller_count * (size_t)state->cfg.taps * sizeof(int16_t));
  memset(state->reference_history, 0,
         (size_t)state->cfg.reference_count * state->cfg.taps *
             sizeof(int16_t));
  memset(state->filtered_x_history, 0,
         controller_count * (size_t)state->cfg.taps * sizeof(int16_t));
  memset(state->secondary_history, 0,
         (size_t)state->cfg.actuator_count * FXLMS_SECONDARY_TAPS *
             sizeof(int16_t));
  memset(state->estimate_history, 0,
         (size_t)state->cfg.reference_count * FXLMS_SECONDARY_ESTIMATE_TAPS *
             sizeof(int16_t));
  state->pos = 0;
  state->secondary_pos = 0;
  state->estimate_pos = 0;
  return FXLMS_OK;
}

static size_t controller_index(const struct fxlms_state *state,
                               uint8_t actuator, uint8_t reference,
                               uint16_t tap) {
  return (((size_t)actuator * state->cfg.reference_count + reference) *
          state->cfg.taps) +
         tap;
}

static size_t reference_index(const struct fxlms_state *state, uint8_t reference,
                              uint16_t tap) {
  return ((size_t)reference * state->cfg.taps) + tap;
}

static size_t filtered_x_index(const struct fxlms_state *state,
                               uint8_t actuator, uint8_t reference,
                               uint16_t tap) {
  return controller_index(state, actuator, reference, tap);
}

int fxlms_process_multi_sample(struct fxlms_state *state,
                               const int16_t *reference_x, int16_t primary_d,
                               struct fxlms_multi_sample_result *out) {
  if (state == NULL || out == NULL) {
    return FXLMS_ERR_INVALID_ARG;
  }

  const uint16_t taps = state->cfg.taps;
  const uint8_t reference_count = state->cfg.reference_count;
  const uint8_t actuator_count = state->cfg.actuator_count;
  if (reference_x == NULL) {
    return FXLMS_ERR_INVALID_ARG;
  }

  for (uint8_t r = 0; r < reference_count; ++r) {
    state->reference_history[reference_index(state, r, state->pos)] =
        reference_x[r];
    state->estimate_history[(size_t)r * FXLMS_SECONDARY_ESTIMATE_TAPS +
                            state->estimate_pos] = reference_x[r];
  }

  int32_t secondary_sum_acc = 0;
  for (uint8_t a = 0; a < actuator_count; ++a) {
    int32_t controller_acc = 0;
    for (uint8_t r = 0; r < reference_count; ++r) {
      controller_acc +=
          fir_q15_raw(&state->weights_g[controller_index(state, a, r, 0)],
                      &state->reference_history[reference_index(state, r, 0)],
                      state->pos, taps, taps);
    }
    int16_t controller_y = sat_i16(controller_acc, state->cfg.output_limit);
    state->secondary_history[(size_t)a * FXLMS_SECONDARY_TAPS +
                             state->secondary_pos] = controller_y;
    int16_t secondary_output =
        fir_q15(k_secondary_path[a],
                &state->secondary_history[(size_t)a * FXLMS_SECONDARY_TAPS],
                state->secondary_pos, FXLMS_SECONDARY_TAPS,
                FXLMS_SECONDARY_TAPS, state->cfg.output_limit);
    out->controller_y[a] = controller_y;
    out->secondary_output[a] = secondary_output;
    secondary_sum_acc += secondary_output;
  }
  for (uint8_t a = actuator_count; a < CONFIG_FXLMS_MAX_ACTUATORS; ++a) {
    out->controller_y[a] = 0;
    out->secondary_output[a] = 0;
  }

  int16_t secondary_sum = sat_i16(secondary_sum_acc, state->cfg.output_limit);
  int16_t error = sat_i16((int32_t)primary_d - secondary_sum,
                          state->cfg.output_limit);

  for (uint8_t a = 0; a < actuator_count; ++a) {
    for (uint8_t r = 0; r < reference_count; ++r) {
      int16_t filtered_x =
          fir_q15(k_secondary_path_estimate[a],
                  &state->estimate_history[(size_t)r *
                                           FXLMS_SECONDARY_ESTIMATE_TAPS],
                  state->estimate_pos, FXLMS_SECONDARY_ESTIMATE_TAPS,
                  FXLMS_SECONDARY_ESTIMATE_TAPS, state->cfg.output_limit);
      state->filtered_x_history[filtered_x_index(state, a, r, state->pos)] =
          filtered_x;
    }
  }

  for (uint8_t a = 0; a < actuator_count; ++a) {
    for (uint8_t r = 0; r < reference_count; ++r) {
      uint16_t hist_idx = state->pos;
      for (uint16_t i = 0; i < taps; ++i) {
        size_t idx = controller_index(state, a, r, i);
        int64_t delta =
            (int64_t)state->cfg.mu_q15 * (int64_t)error *
            (int64_t)state->filtered_x_history[filtered_x_index(
                state, a, r, hist_idx)];
        int32_t updated =
            (int32_t)state->weights_g[idx] +
            shift_trunc_i64(delta, (uint8_t)(30U + state->cfg.adapt_shift));
        state->weights_g[idx] = sat_i16(updated, 32767);
        hist_idx = (hist_idx == 0U) ? (uint16_t)(taps - 1U)
                                    : (uint16_t)(hist_idx - 1U);
      }
    }
  }

  state->pos = (uint16_t)((state->pos + 1U) % taps);
  state->secondary_pos =
      (uint16_t)((state->secondary_pos + 1U) % FXLMS_SECONDARY_TAPS);
  state->estimate_pos =
      (uint16_t)((state->estimate_pos + 1U) % FXLMS_SECONDARY_ESTIMATE_TAPS);

  out->secondary_sum = secondary_sum;
  out->error = error;
  return FXLMS_OK;
}

int fxlms_process_sample(struct fxlms_state *state, int16_t reference_x,
                         int16_t primary_d, struct fxlms_sample_result *out) {
  if (state == NULL || out == NULL || state->cfg.reference_count != 1U ||
      state->cfg.actuator_count != 1U) {
    return FXLMS_ERR_INVALID_ARG;
  }
  struct fxlms_multi_sample_result multi;
  int st = fxlms_process_multi_sample(state, &reference_x, primary_d, &multi);
  if (st != FXLMS_OK) {
    return st;
  }
  out->controller_y = multi.controller_y[0];
  out->secondary_output = multi.secondary_sum;
  out->error = multi.error;
  return FXLMS_OK;
}

int fxlms_process_multi_block(struct fxlms_state *state,
                              const int16_t *reference_x,
                              const int16_t *primary_d, int16_t *error_e,
                              int16_t *controller_y,
                              int16_t *secondary_output, size_t n) {
  if (state == NULL || reference_x == NULL || primary_d == NULL ||
      error_e == NULL) {
    return FXLMS_ERR_INVALID_ARG;
  }

  int16_t sample_refs[CONFIG_FXLMS_MAX_REFERENCES];
  for (size_t i = 0; i < n; ++i) {
    for (uint8_t r = 0; r < state->cfg.reference_count; ++r) {
      sample_refs[r] = reference_x[(size_t)r * n + i];
    }
    struct fxlms_multi_sample_result res;
    int st = fxlms_process_multi_sample(state, sample_refs, primary_d[i], &res);
    if (st != FXLMS_OK) {
      return st;
    }
    error_e[i] = res.error;
    for (uint8_t a = 0; a < state->cfg.actuator_count; ++a) {
      if (controller_y != NULL) {
        controller_y[(size_t)a * n + i] = res.controller_y[a];
      }
      if (secondary_output != NULL) {
        secondary_output[(size_t)a * n + i] = res.secondary_output[a];
      }
    }
  }
  return FXLMS_OK;
}

int fxlms_process_block(struct fxlms_state *state, const int16_t *reference_x,
                        const int16_t *primary_d, int16_t *error_e,
                        int16_t *controller_y, int16_t *secondary_output,
                        size_t n) {
  if (state == NULL || reference_x == NULL || primary_d == NULL ||
      error_e == NULL || state->cfg.reference_count != 1U ||
      state->cfg.actuator_count != 1U) {
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

int fxlms_test_set_multi_controller_tap(struct fxlms_state *state,
                                        uint8_t actuator, uint8_t reference,
                                        uint16_t tap, int16_t value) {
  if (state == NULL || actuator >= state->cfg.actuator_count ||
      reference >= state->cfg.reference_count || tap >= state->cfg.taps) {
    return FXLMS_ERR_INVALID_ARG;
  }
  state->weights_g[controller_index(state, actuator, reference, tap)] = value;
  return FXLMS_OK;
}

int fxlms_test_set_secondary_path(const int16_t *coeffs, size_t n) {
  return fxlms_test_set_actuator_secondary_path(0, coeffs, n);
}

int fxlms_test_set_secondary_estimate(const int16_t *coeffs, size_t n) {
  return fxlms_test_set_actuator_secondary_estimate(0, coeffs, n);
}

int fxlms_test_set_actuator_secondary_path(uint8_t actuator,
                                           const int16_t *coeffs, size_t n) {
  if (coeffs == NULL || n != FXLMS_SECONDARY_TAPS) {
    return FXLMS_ERR_INVALID_ARG;
  }
  if (actuator >= CONFIG_FXLMS_MAX_ACTUATORS) {
    return FXLMS_ERR_INVALID_ARG;
  }
  memcpy(k_secondary_path[actuator], coeffs, sizeof(k_secondary_path[actuator]));
  return FXLMS_OK;
}

int fxlms_test_set_actuator_secondary_estimate(uint8_t actuator,
                                               const int16_t *coeffs,
                                               size_t n) {
  if (coeffs == NULL || n != FXLMS_SECONDARY_ESTIMATE_TAPS) {
    return FXLMS_ERR_INVALID_ARG;
  }
  if (actuator >= CONFIG_FXLMS_MAX_ACTUATORS) {
    return FXLMS_ERR_INVALID_ARG;
  }
  memcpy(k_secondary_path_estimate[actuator], coeffs,
         sizeof(k_secondary_path_estimate[actuator]));
  return FXLMS_OK;
}
#endif
