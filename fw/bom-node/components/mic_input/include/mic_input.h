#ifndef MIC_INPUT_H
#define MIC_INPUT_H

#include <stdbool.h>
#include <stdint.h>

extern volatile bool detection_request;

typedef struct {
  int sampling_freq; // [Hz]
  int pre_event_ms;  // [ms]
  int post_event_ms; // [ms]
} mic_config;

void mic_init(const mic_config *mic_cnfg);
void mic_reader_task(void *arg);
void mic_save_event(int16_t *out_left_mic, int16_t *out_right_mic);

#endif
