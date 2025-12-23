#ifndef PEAK_DETECTOR_H
#define PEAK_DETECTOR_H

#include <stdbool.h>
#include <stdint.h>

enum peak_det_state { PEAK_DET_OK = 0, PEAK_DET_ERR_CFG_UNINITIALIZED = -200 };

// Median peak detector structures
struct median_detector_levels {
  int16_t det_level;
  int16_t det_rms;
  int16_t det_energy;
};

struct median_detector_cfg {
  uint8_t num_taps;
  uint16_t tap_size;
  struct median_detector_levels levels;
};

struct detector_result {
  bool hit;
  int peak_index;
};

// forward declaration for online mode
struct detector_state;

// Functions
/*
 * Returns the required state size for the given configuration (aligned so
 * it can be stored in a uint8_t buffer).
 */
enum peak_det_state detector_state_size(const struct median_detector_cfg *cfg,
                                        size_t *out_size);

/*
 * Initializes state in the caller's buffer.
 * - mem: pointer to pre-allocated buffer
 * - mem_size: size of the buffer
 * - out: receives a valid pointer to state within the buffer
 * Return 0 on success, <0 on error (e.g. insufficient buffer).
 */
int detector_init(void *mem, size_t mem_size,
                  const struct median_detector_cfg *cfg,
                  struct detector_state **out);

void detector_deinit(struct detector_state *s); // optional cleanup
void detector_reset(struct detector_state *s);  // reset execution

// Online processing of a block; block_start_offset = signal offset
int detector_feed_block(struct detector_state *s, const int16_t *block,
                        int64_t block_start_offset,
                        struct detector_result *out);

#endif // PEAK_DETECTOR_H
