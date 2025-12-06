#ifndef IMPULSE_DETECTION_H
#define IMPULSE_DETECTION_H

#include <stdbool.h>
#include <stdint.h>

#ifndef TAP_COUNT
#define TAP_COUNT 11
#endif

#ifndef TAP_SIZE
#define TAP_SIZE 250
#endif

#ifndef DET_LEVEL
#define DET_LEVEL 10000
#endif

#ifndef DET_RMS
#define DET_RMS 2.0f
#endif

#ifndef DET_ENERGY
#define DET_ENERGY 0.1f
#endif

typedef struct {
  uint32_t taps[TAP_COUNT][TAP_SIZE];
  uint32_t sorted_cols[TAP_SIZE][TAP_COUNT];

  uint16_t head;
  uint16_t count;
} impulse_detector;

extern impulse_detector detL;
extern impulse_detector detR;

void impulse_detection_init(impulse_detector *det);

void impulse_add_tap(impulse_detector *det, const int16_t *samples);

bool impulse_run_detection(impulse_detector *det);

#endif
