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

static const char *TAG = "MIC";

static mic_config mic_cfg;
static rb_struct rb_left, rb_right;
i2s_chan_handle_t rx_channel = NULL, tx_channel = NULL;
static bool mic_initialized = false;
#define MIC_TAP_MAX_CALLBACKS 4
static mic_tap_callback tap_cbs[MIC_TAP_MAX_CALLBACKS] = {0};
static void *tap_cb_ctxs[MIC_TAP_MAX_CALLBACKS] = {0};
static int tap_cb_count = 0;

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
  float R0 = expf(-2.0f * 3.1416f * (float)fc_hz / (float)fs);
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
  mic_initialized = true;

  const int samples = mic_cfg.num_taps * mic_cfg.tap_size;
  rb_init(&rb_left, samples);
  rb_init(&rb_right, samples);

  i2s_chan_config_t chan_cfg = {
      .id = I2S_NUM_0,
      .role = I2S_ROLE_MASTER,
      .dma_desc_num = DMA_DESC_NUM,
      .dma_frame_num = CHUNK_FRAMES,
      .auto_clear = true,
  };

  // NOTE: I2S RX was returning zeros unless TX was also enabled, so we
  // create+enable both channels.
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

  // keep TX enabled – see note above
  ESP_ERROR_CHECK(i2s_channel_enable(tx_channel));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_channel));

  int fc = DC_BLOCK_FREQ_HZ;
  dc_block_init(&dcfL, mic_cfg.sampling_freq, fc);
  dc_block_init(&dcfR, mic_cfg.sampling_freq, fc);

  ESP_LOGI(TAG, "I2S initialized");
  ESP_LOGI(TAG, " - Sampling frequency - %d Hz", mic_cfg.sampling_freq);
  ESP_LOGI(TAG, " - Buffer size - %d samples", samples);
}

void mic_init_default(void) {
  mic_config cfg = {
      .sampling_freq = MIC_SAMPLING_FREQUENCY,
      .pre_event_ms = MIC_PRE_EVENT_MS,
      .post_event_ms = MIC_POST_EVENT_MS,
      .num_taps = MIC_DEFAULT_NUM_TAPS,
      .tap_size = MIC_DEFAULT_TAP_SIZE,
  };

  mic_init(&cfg);
}

void mic_start(void) {
  if (!mic_initialized) {
    ESP_LOGE(TAG, "mic_start called before mic_init");
    return;
  }

  xTaskCreatePinnedToCore(mic_reader_task, "mic_reader",
                          MIC_READER_TASK_STACK, NULL,
                          MIC_READER_TASK_PRIORITY, NULL,
                          MIC_READER_TASK_CORE);
}

const mic_config *mic_get_config(void) {
  if (!mic_initialized) {
    return NULL;
  }
  return &mic_cfg;
}

void mic_set_tap_callback(mic_tap_callback cb, void *ctx) {
  tap_cb_count = 0;
  if (cb != NULL) {
    tap_cbs[0] = cb;
    tap_cb_ctxs[0] = ctx;
    tap_cb_count = 1;
  }
}

bool mic_add_tap_callback(mic_tap_callback cb, void *ctx) {
  if (cb == NULL) {
    return false;
  }
  if (tap_cb_count >= MIC_TAP_MAX_CALLBACKS) {
    return false;
  }
  tap_cbs[tap_cb_count] = cb;
  tap_cb_ctxs[tap_cb_count] = ctx;
  tap_cb_count++;
  return true;
}

void mic_reader_task(void *arg) {
  size_t bytes_rec = 0;
  int16_t tapL[mic_cfg.tap_size];
  int16_t tapR[mic_cfg.tap_size];

  while (true) {
    i2s_channel_read(rx_channel, (void *)i2s_read_buffer, READ_BUFFER_BYTES,
                     &bytes_rec, portMAX_DELAY);

    const int n = bytes_rec / 8;
    for (int i = 0; i < n; i++) {

      int32_t sL32 = i2s_read_buffer[2 * i + 1];
      int32_t sR32 = i2s_read_buffer[2 * i + 0];

      int16_t xL0 = int_shift(sL32);
      int16_t xR0 = int_shift(sR32);

      int16_t xL = xL0 + DC_OFFSET_LEFT;
      int16_t xR = xR0 + DC_OFFSET_RIGHT;

      int16_t yL = dc_block_sample(&dcfL, xL);
      int16_t yR = dc_block_sample(&dcfR, xR);

      tapL[i % mic_cfg.tap_size] = yL;
      tapR[i % mic_cfg.tap_size] = yR;

      if (((i + 1) % mic_cfg.tap_size == 0)) {

        for (int j = 0; j < mic_cfg.tap_size; j++) {
          rb_push(&rb_left, tapL[j]);
          rb_push(&rb_right, tapR[j]);
        }

        for (int k = 0; k < tap_cb_count; k++) {
          if (tap_cbs[k]) {
            tap_cbs[k](&tapL[0], &tapR[0], tap_cb_ctxs[k]);
          }
        }
      }
    }
  }
}

void mic_save_event(int16_t *out_left_mic, int16_t *out_right_mic) {

  const int wanted = mic_cfg.num_taps * mic_cfg.tap_size;
  rb_copy_tail(&rb_left, out_left_mic, 0, wanted);
  rb_copy_tail(&rb_right, out_right_mic, 0, wanted);
}
