#ifndef LMS_FILTER_H
#define LMS_FILTER_H

/**
 * @file lms_filter.h
 * @brief Fixed-point classic LMS reference-noise canceller.
 *
 * The filter estimates the noise coupled from a known reference signal into a
 * desired microphone signal. For each input sample it computes:
 *
 *   y[n] = dot(w[n], x[n])
 *   e[n] = d[n] - y[n]
 *   w[n + 1] = w[n] + mu * e[n] * x[n]
 *
 * where x is the reference history, d is the noisy desired signal, y is the
 * estimated coupled noise, and e is the cleaned/error output.
 *
 * Public samples and coefficients use signed Q15-like PCM (`int16_t`) centered
 * around zero. Multiplication and accumulation are done in wider integers, then
 * shifted and saturated back to `int16_t`. The core API does not allocate heap
 * memory; callers provide a state buffer sized by `lms_state_size()`.
 */

#include <stddef.h>
#include <stdint.h>

#if __has_include("lms_config.h")
#include "lms_config.h"
#endif

#ifndef CONFIG_LMS_TAPS
#define CONFIG_LMS_TAPS 64
#endif

#ifndef CONFIG_LMS_MU_Q15
#define CONFIG_LMS_MU_Q15 64
#endif

#ifndef CONFIG_LMS_ADAPT_SHIFT
#define CONFIG_LMS_ADAPT_SHIFT 0
#endif

#ifndef CONFIG_LMS_OUTPUT_LIMIT
#define CONFIG_LMS_OUTPUT_LIMIT 32767
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum lms_status {
  /** Operation completed successfully. */
  LMS_OK = 0,
  /** A required pointer was NULL or an argument combination is invalid. */
  LMS_ERR_INVALID_ARG = -1,
  /** Caller-provided state buffer is smaller than `lms_state_size()`. */
  LMS_ERR_BUFFER_TOO_SMALL = -2,
  /** Configuration fields are outside supported fixed-point limits. */
  LMS_ERR_INVALID_CONFIG = -3
};

/**
 * @brief Runtime copy of LMS configuration.
 *
 * Values are normally populated from generated `CONFIG_LMS_*` macros via
 * `lms_default_config()`. Keeping this structure explicit also allows unit
 * tests and embedded callers to instantiate custom configurations without
 * relying on global state.
 */
struct lms_config {
  /** Number of adaptive FIR taps. Larger values model longer reference paths. */
  uint16_t taps;
  /** Learning rate in Q15. `0` disables adaptation and leaves weights fixed. */
  int16_t mu_q15;
  /** Extra right shift on the weight update for sub-Q15-LSB effective steps. */
  uint8_t adapt_shift;
  /** Absolute saturation limit for estimated noise and cleaned output. */
  int16_t output_limit;
};

/** Opaque filter state stored inside caller-owned memory. */
struct lms_state;

/**
 * @brief Per-sample LMS result.
 */
struct lms_sample_result {
  /** Estimated reference-coupled noise y[n]. */
  int16_t estimated_noise;
  /** Cleaned/error output e[n] = desired - estimated_noise. */
  int16_t error;
};

/**
 * @brief Build the default configuration from generated Kconfig macros.
 */
struct lms_config lms_default_config(void);

/**
 * @brief Return the number of bytes required for `struct lms_state`.
 *
 * The returned size includes the opaque state header, adaptive weights, and
 * reference history ring buffer. The caller may allocate this memory statically
 * or on a controlled embedded heap and pass it to `lms_init()`.
 */
int lms_state_size(const struct lms_config *cfg, size_t *out_size);

/**
 * @brief Initialize an LMS instance in caller-owned memory.
 *
 * @param mem       State storage. Must be at least `lms_state_size()` bytes.
 * @param mem_size  Size of `mem` in bytes.
 * @param cfg       Filter configuration.
 * @param out       Receives the initialized opaque state pointer.
 */
int lms_init(void *mem, size_t mem_size, const struct lms_config *cfg,
             struct lms_state **out);

/**
 * @brief Clear weights and reference history while keeping the same config.
 */
int lms_reset(struct lms_state *state);

/**
 * @brief Process one reference/desired sample pair.
 *
 * `reference` is the measured/known noise reference. `desired` is the noisy
 * microphone signal containing the wanted signal plus reference-coupled noise.
 */
int lms_process_sample(struct lms_state *state, int16_t reference,
                       int16_t desired, struct lms_sample_result *out);

/**
 * @brief Process a contiguous block with an already initialized state.
 *
 * This is suitable for DMA/audio blocks on an MCU. Passing a non-NULL
 * `estimated_noise` buffer is optional and useful for diagnostics on the host.
 */
int lms_process_block(struct lms_state *state, const int16_t *reference,
                      const int16_t *desired, int16_t *cleaned,
                      int16_t *estimated_noise, size_t n);

/**
 * @brief Host convenience wrapper that allocates state and filters one array.
 *
 * This function exists for PC/Python testing. Embedded code should generally
 * use `lms_state_size()`, `lms_init()`, and `lms_process_block()` instead.
 */
int lms_filter_i16(const int16_t *reference, const int16_t *desired,
                   int16_t *cleaned, int16_t *estimated_noise, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LMS_FILTER_H */
