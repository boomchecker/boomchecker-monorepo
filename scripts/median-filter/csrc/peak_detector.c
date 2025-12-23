// Placeholder implementation for peak detector library.
// TODO: add median filter / impulse detection implementation.

#include "peak_detector.h"

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Internal structures for median over tap_size offsets and num_taps taps
struct heap_node {
  int16_t value;
  uint16_t tap_idx;
  uint16_t gen;
};

struct per_offset_median {
  struct heap_node *max_heap;
  struct heap_node *min_heap;
  uint8_t *gen_per_tap; // length num_taps
  size_t max_size;
  size_t min_size;
};

struct detector_state {
  // configuration
  uint8_t num_taps;
  uint16_t tap_size;
  int16_t det_level;
  int16_t det_rms;
  int16_t det_energy;

  // input ring
  uint8_t write_tap;
  int16_t *samples; // length num_taps * tap_size

  // per-offset medians (length tap_size)
  struct per_offset_median *med;

  // RMS / offset
  uint64_t rms_acc;
  uint32_t *sqr_ring; // length num_taps * tap_size
  int64_t base_offset;

  // runtime
  size_t sample_count;

  // generator
  uint16_t current_gen;
};

static size_t align_up(size_t v, size_t a) {
  if (a == 0) {
    return v;
  }
  return (v + a - 1u) & ~(a - 1u);
}

enum peak_det_state detector_state_size(const struct median_detector_cfg *cfg,
                                        size_t *out_size) {
  if (out_size == NULL || cfg == NULL) {
    return PEAK_DET_ERR_CFG_UNINITIALIZED;
  }
  if (cfg->num_taps == 0 || cfg->tap_size == 0) {
    return PEAK_DET_ERR_CFG_UNINITIALIZED;
  }

  // rough overflow check (very simple)
  if (cfg->num_taps > SIZE_MAX / cfg->tap_size) {
    return PEAK_DET_ERR_INVALID_ARG;
  }

  size_t n = (size_t)cfg->num_taps * cfg->tap_size;

  size_t offset = 0;
  offset = align_up(offset, alignof(struct detector_state));
  offset += sizeof(struct detector_state);

  offset = align_up(offset, alignof(int16_t));
  offset += n * sizeof(int16_t); // samples

  offset = align_up(offset, alignof(struct per_offset_median));
  offset += cfg->tap_size * sizeof(struct per_offset_median); // med array

  // per-offset segments
  for (uint16_t i = 0; i < cfg->tap_size; ++i) {
    (void)i;
    offset = align_up(offset, alignof(struct heap_node));
    offset += cfg->num_taps * sizeof(struct heap_node); // max_heap

    offset = align_up(offset, alignof(struct heap_node));
    offset += cfg->num_taps * sizeof(struct heap_node); // min_heap

    offset = align_up(offset, alignof(uint8_t));
    offset += cfg->num_taps * sizeof(uint8_t); // gen_per_tap
  }

  offset = align_up(offset, alignof(uint32_t));
  offset += n * sizeof(uint32_t); // sqr_ring

  *out_size = offset;
  return PEAK_DET_OK;
}

static void layout_state(void *mem_base, const struct median_detector_cfg *cfg,
                         struct detector_state **state_out) {
  uint8_t *base = (uint8_t *)mem_base;
  size_t n = (size_t)cfg->num_taps * cfg->tap_size;
  size_t offset = 0;

  offset = align_up(offset, alignof(struct detector_state));
  struct detector_state *s = (struct detector_state *)(base + offset);
  offset += sizeof(struct detector_state);

  offset = align_up(offset, alignof(int16_t));
  s->samples = (int16_t *)(base + offset);
  offset += n * sizeof(int16_t);

  offset = align_up(offset, alignof(struct per_offset_median));
  s->med = (struct per_offset_median *)(base + offset);
  offset += cfg->tap_size * sizeof(struct per_offset_median);

  for (uint16_t i = 0; i < cfg->tap_size; ++i) {
    offset = align_up(offset, alignof(struct heap_node));
    s->med[i].max_heap = (struct heap_node *)(base + offset);
    offset += cfg->num_taps * sizeof(struct heap_node);

    offset = align_up(offset, alignof(struct heap_node));
    s->med[i].min_heap = (struct heap_node *)(base + offset);
    offset += cfg->num_taps * sizeof(struct heap_node);

    offset = align_up(offset, alignof(uint8_t));
    s->med[i].gen_per_tap = (uint8_t *)(base + offset);
    offset += cfg->num_taps * sizeof(uint8_t);
  }

  offset = align_up(offset, alignof(uint32_t));
  s->sqr_ring = (uint32_t *)(base + offset);

  *state_out = s;
}

enum peak_det_state detector_init(void *mem, size_t mem_size,
                                  const struct median_detector_cfg *cfg,
                                  struct detector_state **out) {
  if (mem == NULL || out == NULL || cfg == NULL) {
    return PEAK_DET_ERR_CFG_UNINITIALIZED;
  }
  if (cfg->num_taps == 0 || cfg->tap_size == 0) {
    return PEAK_DET_ERR_CFG_UNINITIALIZED;
  }

  size_t needed = 0;
  enum peak_det_state st = detector_state_size(cfg, &needed);
  if (st != PEAK_DET_OK) {
    return st;
  }
  if (mem_size < needed) {
    return PEAK_DET_ERR_BUFFER_TOO_SMALL;
  }

  memset(mem, 0, needed);
  struct detector_state *state = NULL;
  layout_state(mem, cfg, &state);

  state->num_taps = cfg->num_taps;
  state->tap_size = cfg->tap_size;
  state->det_level = cfg->levels.det_level;
  state->det_rms = cfg->levels.det_rms;
  state->det_energy = cfg->levels.det_energy;
  state->write_tap = 0;
  state->current_gen = 1;
  state->rms_acc = 0;

  // initialize per-offset median structures
  for (uint16_t i = 0; i < cfg->tap_size; ++i) {
    state->med[i].max_size = 0;
    state->med[i].min_size = 0;
    // gen_per_tap already zeroed by memset
  }

  *out = state;
  return PEAK_DET_OK;
}

void detector_deinit(struct detector_state *s) {
  (void)s; // nothing to free, buffer owned by caller
}

void detector_reset(struct detector_state *s) {
  if (!s) {
    return;
  }
  size_t total = (size_t)s->num_taps * s->tap_size;
  memset(s->samples, 0, total * sizeof(int16_t));
  memset(s->sqr_ring, 0, total * sizeof(uint32_t));
  for (uint16_t i = 0; i < s->tap_size; ++i) {
    s->med[i].max_size = 0;
    s->med[i].min_size = 0;
    memset(s->med[i].gen_per_tap, 0, s->num_taps * sizeof(uint8_t));
  }
  s->write_tap = 0;
  s->current_gen = 1;
  s->rms_acc = 0;
  s->sample_count = 0;
  s->base_offset = 0;
}

int detector_feed_block(struct detector_state *s, const int16_t *block,
                        int64_t block_start_offset,
                        struct detector_result *out) {
  (void)s;
  (void)block;
  (void)block_start_offset;
  (void)out;
  // TODO: implement median update and detection logic
  return PEAK_DET_ERR_INVALID_ARG;
}
