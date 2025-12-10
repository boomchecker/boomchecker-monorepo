#ifndef MIC_INPUT_H
#define MIC_INPUT_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include <stdint.h>

#define I2S_BCLK_IO GPIO_NUM_19
#define I2S_WS_IO GPIO_NUM_18
#define I2S_DIN_IO GPIO_NUM_21

#define DMA_DESC_NUM 14
#define CHUNK_FRAMES 511
#define READ_BUFFER_BYTES (CHUNK_FRAMES * 8)
// 1 frame = L(32b) + R(32b) = 8 B

// DC offset correction values for left and right microphone channels.
// Units: ADC counts.
// These values were determined empirically by measuring the average DC bias
// present on each channel during calibration with no input signal.
// They are needed to remove the DC component from the microphone signal,
// ensuring accurate audio processing and event detection.
#define DC_OFFSET_LEFT 3500
#define DC_OFFSET_RIGHT 3000
#define DC_BLOCK_FREQ_HZ 100

extern bool detection_request;

typedef struct {
  int sampling_freq; // [Hz]
  int pre_event_ms;  // [ms]
  int post_event_ms; // [ms]
  int num_taps;
  int tap_size;
} mic_config;

void mic_init(const mic_config *mic_cnfg);
void mic_reader_task(void *arg);
void mic_save_event(int16_t *out_left_mic, int16_t *out_right_mic);

#endif
