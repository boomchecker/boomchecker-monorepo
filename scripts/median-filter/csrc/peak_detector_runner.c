#include "peak_detector.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

int detect_recording_i16(const int16_t *samples, size_t n,
                         const struct median_detector_cfg *cfg, int *positions,
                         size_t capacity) {
  if (samples == NULL || cfg == NULL) {
    return PEAK_DET_ERR_INVALID_ARG;
  }
  size_t needed = 0;
  enum peak_det_state st = detector_state_size(cfg, &needed);
  if (st != PEAK_DET_OK) {
    return st;
  }
  uint8_t *buf = (uint8_t *)malloc(needed);
  if (buf == NULL) {
    return PEAK_DET_ERR_BUFFER_TOO_SMALL;
  }
  struct detector_state *state = NULL;
  st = detector_init(buf, needed, cfg, &state);
  if (st != PEAK_DET_OK) {
    free(buf);
    return st;
  }

  size_t hits = 0;
  int64_t offset = 0;
  for (size_t i = 0; i + cfg->tap_size <= n; i += cfg->tap_size) {
    struct detector_result res;
    st = detector_feed_block(state, samples + i, offset, &res);
    if (st != PEAK_DET_OK) {
      detector_deinit(state);
      free(buf);
      return st;
    }
    if (res.hit && positions != NULL && hits < capacity) {
      positions[hits] = res.peak_index;
    }
    if (res.hit) {
      ++hits;
    }
    offset += cfg->tap_size;
  }

  detector_deinit(state);
  free(buf);
  return (int)hits;
}
