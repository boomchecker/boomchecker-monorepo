
#include "mic_input.h"
#include "ring_buffer.h"

#include "driver/i2s.h"
// #include "driver/i2s_std.h"
// #include "driver/i2s_types.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

#define I2S_LEFT_RIGHT I2S_NUM_0

static const i2s_pin_config_t pins0 = {
    .bck_io_num = 19,
    .ws_io_num = 18,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = 21,
    .mck_io_num = I2S_PIN_NO_CHANGE,
};

static mic_config mic_cfg;
static rb_struct rb_left, rb_right;

static inline int int_shift(int s) {
  int ret = s >> 14;
  return ret;
}

void mic_init(const mic_config *cfg) {
  mic_cfg = *cfg;

  // alocation for ring buffers
  int event_length_ms = mic_cfg.pre_event_ms + mic_cfg.post_event_ms;
  int samples = mic_cfg.sampling_freq * 4 * event_length_ms / 1000;
  rb_init(&rb_left, samples);
  rb_init(&rb_right, samples);

  i2s_config_t general = {
      .mode = I2S_MODE_MASTER | I2S_MODE_RX,
      .sample_rate = mic_cfg.sampling_freq,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S,
      .dma_buf_count = 14,
      .dma_buf_len = 1024,
      .use_apll = false,
      .intr_alloc_flags = 0,
  };

  ESP_ERROR_CHECK(i2s_driver_install(I2S_LEFT_RIGHT, &general, 0, NULL));
  ESP_ERROR_CHECK(i2s_set_pin(I2S_LEFT_RIGHT, &pins0));
  ESP_ERROR_CHECK(i2s_zero_dma_buffer(I2S_LEFT_RIGHT));

  ESP_LOGI("MIC", "I2S initialized");
  ESP_LOGI("MIC", " - Sampling frequency - %d Hz", mic_cfg.sampling_freq);
  ESP_LOGI("MIC", " - Buffer size - %d samples", samples);
}

static void mic_reader_task(void *arg) {
  const size_t CHUNK = 512;
  int *buf0 = (int *)malloc(CHUNK * sizeof(int));

  if (!buf0) {
    ESP_LOGE("MIC", "malloc failed");
    vTaskDelete(NULL);
  }

  size_t bytes_recieved0 = 0;

  while (true) {
    i2s_read(I2S_LEFT_RIGHT, buf0, CHUNK * sizeof(int), &bytes_recieved0,
             portMAX_DELAY);

    int n0 = bytes_recieved0 / 8;

    for (int i = 0; i < n0; ++i) {
      int sL = buf0[2 * i + 0];
      int sR = buf0[2 * i + 1];
      rb_push(&rb_left, int_shift(sL));
      rb_push(&rb_right, int_shift(sR));
    }
  }
}

void mic_start_reading(void) {
  xTaskCreatePinnedToCore(mic_reader_task, "mic_reader", 8192, NULL, 5, NULL,
                          0);
  ESP_LOGI("MIC", "Starting mic reader task");
}

void mic_save_event(int *out_left_mic, int *out_right_mic) {

  const int pre_samples = mic_cfg.sampling_freq * mic_cfg.pre_event_ms / 1000;
  const int post_samples = mic_cfg.sampling_freq * mic_cfg.post_event_ms / 1000;

  const int wanted_samples = pre_samples + post_samples;

  rb_copy_tail(&rb_left, out_left_mic, 0, wanted_samples);
  rb_copy_tail(&rb_right, out_right_mic, 0, wanted_samples);

  ESP_LOGI("MIC", "Event saved");
}
