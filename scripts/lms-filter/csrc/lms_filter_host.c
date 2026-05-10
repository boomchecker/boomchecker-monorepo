#include "lms_filter.h"

#include <stdint.h>
#include <stdlib.h>

int fxlms_filter_i16(const int16_t *reference_x, const int16_t *primary_d,
                     int16_t *error_e, int16_t *controller_y,
                     int16_t *secondary_output, size_t n) {
  if (reference_x == NULL || primary_d == NULL || error_e == NULL) {
    return FXLMS_ERR_INVALID_ARG;
  }

  struct fxlms_config cfg = fxlms_default_config();
  size_t needed = 0;
  int st = fxlms_state_size(&cfg, &needed);
  if (st != FXLMS_OK) {
    return st;
  }

  void *mem = calloc(1, needed);
  if (mem == NULL) {
    return FXLMS_ERR_BUFFER_TOO_SMALL;
  }

  struct fxlms_state *state = NULL;
  st = fxlms_init(mem, needed, &cfg, &state);
  if (st == FXLMS_OK) {
    st = fxlms_process_block(state, reference_x, primary_d, error_e,
                             controller_y, secondary_output, n);
  }

  free(mem);
  return st;
}
