#include "mic_input.h"
#include "ring_buffer.h"

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/i2s_types.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define I2S_BCLK_IO GPIO_NUM_19
#define I2S_WS_IO GPIO_NUM_18
#define I2S_DIN_IO GPIO_NUM_21

#define DC_BLOCK_FREQ_HZ 20

static const char *TAG = "MIC";

static mic_config mic_cfg;
static rb_struct rb_left, rb_right;
i2s_chan_handle_t rx_channel = NULL, tx_channel = NULL;

#define CHUNK_FRAMES 1024
#define READ_BUFFER_BYTES (CHUNK_FRAMES * 8)
// 1 frame = L(32b) + R(32b) = 8 B

static int32_t i2s_read_buffer[CHUNK_FRAMES * 2];

typedef struct {
  int32_t x1;
  int32_t y1;
  int32_t R;
} dc_filter;

static dc_filter dcfL = {0}, dcfR = {0};

static inline int16_t dc_block_sample(dc_filter *st, int16_t x) {
  int32_t xn = (int32_t)x;
  int32_t yn = xn - st->x1 + ((st->R * st->y1) >> 15);
  st->x1 = xn;
  st->y1 = yn;
  return (int16_t)yn;
}

static void dc_block_init(dc_filter *st, int fs, int fc_hz) {
  if (fc_hz <= 0)
    fc_hz = 20;
  float R0 = expf(-2.0f * 3.14159265359f * (float)fc_hz / (float)fs);
  int32_t R = (int32_t)(R0 * 32768.0f + 0.5f);
  if (R > 32767)
    R = 32767;
  if (R < 0)
    R = 0;
  st->x1 = 0;
  st->y1 = 0;
  st->R = R;
}

static inline int16_t int_shift(int32_t s) { return (int16_t)(s >> 16); }

void mic_init(const mic_config *cfg) {
  mic_cfg = *cfg;

  const int event_length_ms = mic_cfg.pre_event_ms + mic_cfg.post_event_ms;
  const int samples = (mic_cfg.sampling_freq * event_length_ms) / 1000;
  rb_init(&rb_left, samples);
  rb_init(&rb_right, samples);

  i2s_chan_config_t chan_cfg = {
      .id = I2S_NUM_0,
      .role = I2S_ROLE_MASTER,
      .dma_desc_num = 14,
      .dma_frame_num = 1024,
      .auto_clear = true,
  };

  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_channel, &rx_channel));

  i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
  slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;

  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(mic_cfg.sampling_freq),
      .slot_cfg = slot_cfg,
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = GPIO_NUM_19,
              .ws = GPIO_NUM_18,
              .dout = I2S_GPIO_UNUSED,
              .din = GPIO_NUM_21,
              .invert_flags =
                  {
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_channel, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_channel, &std_cfg));

  ESP_ERROR_CHECK(i2s_channel_enable(tx_channel));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_channel));

  int fc = DC_BLOCK_FREQ_HZ;
  dc_block_init(&dcfL, mic_cfg.sampling_freq, fc);
  dc_block_init(&dcfR, mic_cfg.sampling_freq, fc);

  ESP_LOGI("MIC", "I2S initialized");
  ESP_LOGI("MIC", " - Sampling frequency - %d Hz", mic_cfg.sampling_freq);
  ESP_LOGI("MIC", " - Buffer size - %d samples", samples);
}

void mic_reader_task(void *arg) {
  size_t bytes_rec = 0;

  while (true) {
    i2s_channel_read(rx_channel, (void *)i2s_read_buffer, READ_BUFFER_BYTES,
                     &bytes_rec, portMAX_DELAY);

    const int n = bytes_rec / 8;
    for (int i = 0; i < n; ++i) {
      int32_t sL32 = i2s_read_buffer[2 * i + 0];
      int32_t sR32 = i2s_read_buffer[2 * i + 1];

      int16_t xL = int_shift(sL32);
      int16_t xR = int_shift(sR32);

      int16_t yL = dc_block_sample(&dcfL, xL);
      int16_t yR = dc_block_sample(&dcfR, xR);

      rb_push(&rb_left, yL);
      rb_push(&rb_right, yR);
    }
  }
}

void mic_save_event(int16_t *out_left_mic, int16_t *out_right_mic) {
  const int pre_samples = (mic_cfg.sampling_freq * mic_cfg.pre_event_ms) / 1000;
  const int post_samples =
      (mic_cfg.sampling_freq * mic_cfg.post_event_ms) / 1000;
  const int wanted = pre_samples + post_samples;

  rb_copy_tail(&rb_left, out_left_mic, 0, wanted);
  rb_copy_tail(&rb_right, out_right_mic, 0, wanted);

  ESP_LOGI(TAG, "Event uložen (%d vzorků/kanál)", wanted);
}
