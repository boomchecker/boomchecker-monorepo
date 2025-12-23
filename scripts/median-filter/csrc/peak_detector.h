#ifndef PEAK_DETECTOR_H
#define PEAK_DETECTOR_H

#include <stdbool.h>
#include <stdint.h>

// Median peak detector structures
struct {
  int16_t det_level;
  int16_t det_rms;
  int16_t det_energy;
} median_detector_levels;

struct {
  uint8_t num_taps;
  uint16_t tap_size;
  struct median_detector_levels levels;
} median_detector_cfg;

struct {
  bool hit;
  int peak_index;
} detector_result;

// forward declaration for online mode
struct detector_state detector_state;

// Functions
struct detector_state *detector_create(const struct median_detector_cfg *cfg);
bool detector_destroy(struct detector_state *s);
int detector_feed_block(struct detector_state *s, const int16_t *block,
                        int64_t block_start_offset,
                        struct detector_result *out);
int detect_recording_i16(const int16_t *samples, size_t n,
                         const struct median_detector_cfg *cfg, int *positions,
                         size_t capacity);

#endif // PEAK_DETECTOR_H
