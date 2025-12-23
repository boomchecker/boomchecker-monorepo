// Placeholder implementation for peak detector library.
// TODO: add median filter / impulse detection implementation.

#include "peak_detector.h"

#include <stdalign.h>

static size_t align_up(size_t v, size_t a) { return (v + a - 1) & ~(a - 1); }

enum peak_det_state detector_state_size(const struct median_detector_cfg *cfg,
                                        size_t *out_size) {
  if (!out_size)
    return PEAK_DET_ERR_CFG_UNINITIALIZED;
  if (!cfg)
    return PEAK_DET_ERR_CFG_UNINITIALIZED;
  if (cfg->num_taps == 0 || cfg->tap_size == 0)
    return PEAK_DET_ERR_CFG_UNINITIALIZED;

  size_t n = (size_t)cfg->num_taps * cfg->tap_size;

  size_t sz = 0;
  sz = align_up(sz + sizeof(struct detector_state_fixed), alignof(int16_t));
  sz = align_up(sz + n * sizeof(int16_t), alignof(heap_node)); // kruhový buffer
  sz = align_up(sz + cfg->tap_size * sizeof(heap_node),
                alignof(heap_node)); // max-heap
  sz = align_up(sz + cfg->tap_size * sizeof(heap_node),
                alignof(heap_node)); // min-heap

  *out_size = sz;

  return PEAK_DET_OK;
}