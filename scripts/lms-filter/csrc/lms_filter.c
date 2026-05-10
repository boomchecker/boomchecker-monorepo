#include "lms_filter.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct lms_state {
  struct lms_config cfg;
  uint16_t pos;
  /* Q15 adaptive FIR coefficients. */
  int16_t *weights;
  /* Ring buffer with the newest reference sample at `pos`. */
  int16_t *history;
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

static bool valid_config(const struct lms_config *cfg) {
  return cfg != NULL && cfg->taps > 0U && cfg->taps <= 512U && cfg->mu_q15 >= 0 &&
         cfg->adapt_shift <= 30U && cfg->output_limit > 0;
}

struct lms_config lms_default_config(void) {
  struct lms_config cfg;
  cfg.taps = (uint16_t)CONFIG_LMS_TAPS;
  cfg.mu_q15 = (int16_t)CONFIG_LMS_MU_Q15;
  cfg.adapt_shift = (uint8_t)CONFIG_LMS_ADAPT_SHIFT;
  cfg.output_limit = (int16_t)CONFIG_LMS_OUTPUT_LIMIT;
  return cfg;
}

int lms_state_size(const struct lms_config *cfg, size_t *out_size) {
  if (out_size == NULL) {
    return LMS_ERR_INVALID_ARG;
  }
  if (!valid_config(cfg)) {
    return LMS_ERR_INVALID_CONFIG;
  }

  size_t offset = align_up(sizeof(struct lms_state), _Alignof(int16_t));
  offset += (size_t)cfg->taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  offset += (size_t)cfg->taps * sizeof(int16_t);
  *out_size = offset;
  return LMS_OK;
}

int lms_init(void *mem, size_t mem_size, const struct lms_config *cfg,
             struct lms_state **out) {
  if (mem == NULL || out == NULL) {
    return LMS_ERR_INVALID_ARG;
  }
  size_t needed = 0;
  int st = lms_state_size(cfg, &needed);
  if (st != LMS_OK) {
    return st;
  }
  if (mem_size < needed) {
    return LMS_ERR_BUFFER_TOO_SMALL;
  }

  memset(mem, 0, needed);
  struct lms_state *state = (struct lms_state *)mem;
  size_t offset = align_up(sizeof(struct lms_state), _Alignof(int16_t));
  state->weights = (int16_t *)((uint8_t *)mem + offset);
  offset += (size_t)cfg->taps * sizeof(int16_t);
  offset = align_up(offset, _Alignof(int16_t));
  state->history = (int16_t *)((uint8_t *)mem + offset);
  state->cfg = *cfg;
  state->pos = 0;
  *out = state;
  return LMS_OK;
}

int lms_reset(struct lms_state *state) {
  if (state == NULL) {
    return LMS_ERR_INVALID_ARG;
  }
  memset(state->weights, 0, (size_t)state->cfg.taps * sizeof(int16_t));
  memset(state->history, 0, (size_t)state->cfg.taps * sizeof(int16_t));
  state->pos = 0;
  return LMS_OK;
}

int lms_process_sample(struct lms_state *state, int16_t reference,
                       int16_t desired, struct lms_sample_result *out) {
  if (state == NULL || out == NULL) {
    return LMS_ERR_INVALID_ARG;
  }

  const uint16_t taps = state->cfg.taps;
  state->history[state->pos] = reference;

  /* Estimate coupled noise y[n] = dot(weights, reference_history). */
  int64_t y_acc = 0;
  uint16_t hist_idx = state->pos;
  for (uint16_t i = 0; i < taps; ++i) {
    y_acc += (int32_t)state->weights[i] * (int32_t)state->history[hist_idx];
    hist_idx = (hist_idx == 0U) ? (uint16_t)(taps - 1U) : (uint16_t)(hist_idx - 1U);
  }

  int16_t y = sat_i16((int32_t)(y_acc >> 15), state->cfg.output_limit);
  int16_t e = sat_i16((int32_t)desired - (int32_t)y, state->cfg.output_limit);

  /*
   * Classic LMS coefficient update in fixed-point form:
   *
   *   delta_w = mu_q15 * e_q15 * x_q15 >> (30 + adapt_shift)
   *
   * The first 30 bits compensate Q15*Q15*Q15 scaling. `adapt_shift` provides
   * finer effective learning rates than the smallest non-zero Q15 mu value.
   */
  hist_idx = state->pos;
  for (uint16_t i = 0; i < taps; ++i) {
    int64_t delta = (int64_t)state->cfg.mu_q15 * (int64_t)e *
                    (int64_t)state->history[hist_idx];
    int32_t updated =
        (int32_t)state->weights[i] + (int32_t)(delta >> (30U + state->cfg.adapt_shift));
    state->weights[i] = sat_i16(updated, 32767);
    hist_idx = (hist_idx == 0U) ? (uint16_t)(taps - 1U) : (uint16_t)(hist_idx - 1U);
  }

  state->pos = (uint16_t)((state->pos + 1U) % taps);
  out->estimated_noise = y;
  out->error = e;
  return LMS_OK;
}

int lms_process_block(struct lms_state *state, const int16_t *reference,
                      const int16_t *desired, int16_t *cleaned,
                      int16_t *estimated_noise, size_t n) {
  if (state == NULL || reference == NULL || desired == NULL || cleaned == NULL) {
    return LMS_ERR_INVALID_ARG;
  }
  for (size_t i = 0; i < n; ++i) {
    struct lms_sample_result res;
    int st = lms_process_sample(state, reference[i], desired[i], &res);
    if (st != LMS_OK) {
      return st;
    }
    cleaned[i] = res.error;
    if (estimated_noise != NULL) {
      estimated_noise[i] = res.estimated_noise;
    }
  }
  return LMS_OK;
}
