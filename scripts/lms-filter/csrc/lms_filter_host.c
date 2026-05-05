#include "lms_filter.h"

#include <stdint.h>
#include <stdlib.h>

int lms_filter_i16(const int16_t *reference, const int16_t *desired,
                   int16_t *cleaned, int16_t *estimated_noise, size_t n) {
  if (reference == NULL || desired == NULL || cleaned == NULL) {
    return LMS_ERR_INVALID_ARG;
  }

  struct lms_config cfg = lms_default_config();
  size_t needed = 0;
  int st = lms_state_size(&cfg, &needed);
  if (st != LMS_OK) {
    return st;
  }

  void *mem = calloc(1, needed);
  if (mem == NULL) {
    return LMS_ERR_BUFFER_TOO_SMALL;
  }

  struct lms_state *state = NULL;
  st = lms_init(mem, needed, &cfg, &state);
  if (st == LMS_OK) {
    st = lms_process_block(state, reference, desired, cleaned, estimated_noise, n);
  }

  free(mem);
  return st;
}

