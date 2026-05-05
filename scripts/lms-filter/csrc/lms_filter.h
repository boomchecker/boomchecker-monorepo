#ifndef LMS_FILTER_H
#define LMS_FILTER_H

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
  LMS_OK = 0,
  LMS_ERR_INVALID_ARG = -1,
  LMS_ERR_BUFFER_TOO_SMALL = -2,
  LMS_ERR_INVALID_CONFIG = -3
};

struct lms_config {
  uint16_t taps;
  int16_t mu_q15;
  uint8_t adapt_shift;
  int16_t output_limit;
};

struct lms_state;

struct lms_sample_result {
  int16_t estimated_noise;
  int16_t error;
};

struct lms_config lms_default_config(void);
int lms_state_size(const struct lms_config *cfg, size_t *out_size);
int lms_init(void *mem, size_t mem_size, const struct lms_config *cfg,
             struct lms_state **out);
int lms_reset(struct lms_state *state);
int lms_process_sample(struct lms_state *state, int16_t reference,
                       int16_t desired, struct lms_sample_result *out);
int lms_process_block(struct lms_state *state, const int16_t *reference,
                      const int16_t *desired, int16_t *cleaned,
                      int16_t *estimated_noise, size_t n);

int lms_filter_i16(const int16_t *reference, const int16_t *desired,
                   int16_t *cleaned, int16_t *estimated_noise, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LMS_FILTER_H */
