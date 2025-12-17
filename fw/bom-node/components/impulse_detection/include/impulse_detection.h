#ifndef IMPULSE_DETECTION_H
#define IMPULSE_DETECTION_H

#include <stdbool.h>
#include <stdint.h>

#ifndef TAP_COUNT
#define TAP_COUNT 31
#endif

#ifndef TAP_SIZE
#define TAP_SIZE 30
#endif

// DET_LEVEL represents the squared amplitude threshold for impulse detection.
// Units: (amplitude)^2, typically derived from the squared value of the input
// signal. Calibration: Set based on expected signal amplitude; increase to
// reduce sensitivity.
#ifndef DET_LEVEL
#define DET_LEVEL 10000
#endif

// DET_RMS is a multiplier applied to the RMS (root mean square) value of the
// signal. Meaning: Used to scale the RMS value for threshold comparison.
// Calibration: Adjust to tune detection sensitivity; higher values require
// stronger impulses.
#ifndef DET_RMS
#define DET_RMS 100
#endif

// DET_ENERGY is the minimum ratio of energy after an impulse compared to
// before. Meaning: Used to compare energy levels before and after a detected
// impulse. Calibration: Set between 0 and 1; lower values allow weaker impulses
// to be detected.
#ifndef DET_ENERGY
#define DET_ENERGY 0.4f
#endif

// Data structure for the median-based impulse detection algorithm. This
// structure maintains a circular buffer of "taps"(signal segments) and their
// corresponding sorted values to facilitate real - time median filtering.
typedef struct {
  // Circular buffer of squared signal samples. Organized as [TAP_COUNT]
  // segments, each containing [TAP_SIZE] samples. The total sliding
  // window size is TAP_COUNT *TAP_SIZE samples.
  uint32_t taps[TAP_COUNT][TAP_SIZE];

  // Matrix of sorted samples used for fast median calculation.
  // Each column[i] contains TAP_COUNT sorted samples from the i-th position of
  // all currently stored taps.
  uint32_t sorted_cols[TAP_SIZE][TAP_COUNT];

  // Index of the most recently written tap in the circular buffer.
  uint16_t head;

  // Number of taps currently stored (up to TAP_COUNT).
  uint16_t count;
} impulse_detector;

extern impulse_detector detL;
extern impulse_detector detR;

void impulse_detection_init(impulse_detector *det);

void impulse_add_tap(impulse_detector *det, const int16_t *samples);

bool impulse_run_detection(impulse_detector *det);

#endif
