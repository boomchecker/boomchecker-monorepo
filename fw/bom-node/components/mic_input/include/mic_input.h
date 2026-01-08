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
// CHUNK_FRAMES is set to 511 due to DMA buffer constraints on the hardware.
// 511 is the maximum number of frames that can be processed in one chunk
// without exceeding DMA limits.
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

// DC_BLOCK_FREQ_HZ sets the cutoff frequency for the high-pass filter used to
// remove DC offset from the microphone signal. The value was increased from 20
// Hz to 100 Hz to more aggressively filter out low-frequency noise and DC
// drift, which can interfere with impulse detection. A higher cutoff improves
// the algorithm's sensitivity to short, transient impulses by reducing baseline
// fluctuations, but may attenuate very low-frequency events. 100 Hz was chosen
// as a balance between effective DC removal and preserving relevant impulse
// features.
#define DC_BLOCK_FREQ_HZ 100

#ifndef MIC_SAMPLING_FREQUENCY
#define MIC_SAMPLING_FREQUENCY 44100
#endif

#ifndef MIC_PRE_EVENT_MS
#define MIC_PRE_EVENT_MS 10
#endif

#ifndef MIC_POST_EVENT_MS
#define MIC_POST_EVENT_MS 10
#endif

#ifndef MIC_DEFAULT_NUM_TAPS
#define MIC_DEFAULT_NUM_TAPS 31
#endif

#ifndef MIC_DEFAULT_TAP_SIZE
#define MIC_DEFAULT_TAP_SIZE 30
#endif

#ifndef MIC_READER_TASK_STACK
#define MIC_READER_TASK_STACK 8192
#endif

#ifndef MIC_READER_TASK_PRIORITY
#define MIC_READER_TASK_PRIORITY 5
#endif

#ifndef MIC_READER_TASK_CORE
#define MIC_READER_TASK_CORE 0
#endif

typedef struct {
  int sampling_freq; // [Hz]
  int pre_event_ms;  // [ms]
  int post_event_ms; // [ms]
  int num_taps;
  int tap_size;
} mic_config;

void mic_init(const mic_config *mic_cnfg);
void mic_init_default(void);
void mic_start(void);
const mic_config *mic_get_config(void);
void mic_reader_task(void *arg);
void mic_save_event(int16_t *out_left_mic, int16_t *out_right_mic);

typedef void (*mic_tap_callback)(const int16_t *tap_left,
                                 const int16_t *tap_right, void *ctx);
void mic_set_tap_callback(mic_tap_callback cb, void *ctx);

#endif
