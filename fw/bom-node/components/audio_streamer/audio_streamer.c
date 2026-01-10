#include "audio_streamer.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mic_input.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static const char *TAG = "AUDIO_STREAM";

#define STREAM_CHUNK_FRAMES 480
#define STREAM_QUEUE_LENGTH 8
#define STREAM_TASK_STACK   6144
#define STREAM_TASK_PRIO    5
#define STREAM_RETRY_MS     1000

typedef struct {
  size_t bytes;
  int16_t data[STREAM_CHUNK_FRAMES * 2];
} audio_chunk_t;

static QueueHandle_t s_queue = NULL;
static TaskHandle_t s_task = NULL;
static SemaphoreHandle_t s_cfg_mutex = NULL;
static audio_config_t s_config = {0};
static volatile bool s_enabled = false;
static bool s_need_reconnect = false;
static int s_tap_size = 0;
static int s_sample_rate = 0;
static audio_chunk_t s_accum_chunk = {0};
static size_t s_accum_frames = 0;

static void write_le16(uint8_t *dst, uint16_t val) {
  dst[0] = (uint8_t)(val & 0xff);
  dst[1] = (uint8_t)((val >> 8) & 0xff);
}

static void write_le32(uint8_t *dst, uint32_t val) {
  dst[0] = (uint8_t)(val & 0xff);
  dst[1] = (uint8_t)((val >> 8) & 0xff);
  dst[2] = (uint8_t)((val >> 16) & 0xff);
  dst[3] = (uint8_t)((val >> 24) & 0xff);
}

static bool audio_streamer_mode_ok(const char *mode) {
  if (mode == NULL) {
    return false;
  }
  return (strcmp(mode, "http") == 0) || (strcmp(mode, "http_stream") == 0);
}

static bool audio_streamer_should_stream(const audio_config_t *cfg) {
  return cfg->enabled && audio_streamer_mode_ok(cfg->mode) &&
         cfg->upload_url[0] != '\0';
}

static void audio_streamer_build_wav_header(uint8_t *out, int sample_rate) {
  const uint16_t num_channels = 2;
  const uint16_t bits_per_sample = 16;
  const uint32_t byte_rate = sample_rate * num_channels * bits_per_sample / 8;
  const uint16_t block_align = num_channels * bits_per_sample / 8;
  const uint32_t data_size = 0xffffffff;
  const uint32_t riff_size = data_size + 36;

  memcpy(out, "RIFF", 4);
  write_le32(out + 4, riff_size);
  memcpy(out + 8, "WAVE", 4);
  memcpy(out + 12, "fmt ", 4);
  write_le32(out + 16, 16);
  write_le16(out + 20, 1);
  write_le16(out + 22, num_channels);
  write_le32(out + 24, (uint32_t)sample_rate);
  write_le32(out + 28, byte_rate);
  write_le16(out + 32, block_align);
  write_le16(out + 34, bits_per_sample);
  memcpy(out + 36, "data", 4);
  write_le32(out + 40, data_size);
}

static void audio_streamer_on_tap(const int16_t *tap_left,
                                  const int16_t *tap_right, void *ctx) {
  (void)ctx;
  if (!s_enabled || s_queue == NULL || s_tap_size <= 0) {
    s_accum_frames = 0;
    return;
  }

  for (int i = 0; i < s_tap_size; i++) {
    s_accum_chunk.data[s_accum_frames * 2] = tap_left[i];
    s_accum_chunk.data[s_accum_frames * 2 + 1] = tap_right[i];
    s_accum_frames++;

    if (s_accum_frames >= STREAM_CHUNK_FRAMES) {
      s_accum_chunk.bytes = STREAM_CHUNK_FRAMES * 2 * sizeof(int16_t);
      if (xQueueSend(s_queue, &s_accum_chunk, 0) != pdTRUE) {
        // Drop chunk when queue is full to keep the mic reader unblocked.
      }
      s_accum_frames = 0;
    }
  }
}

static void audio_streamer_copy_config(audio_config_t *out, bool *need_reconnect) {
  if (xSemaphoreTake(s_cfg_mutex, portMAX_DELAY) == pdTRUE) {
    *out = s_config;
    if (need_reconnect) {
      *need_reconnect = s_need_reconnect;
      s_need_reconnect = false;
    }
    xSemaphoreGive(s_cfg_mutex);
  }
}

static void audio_streamer_task(void *arg) {
  (void)arg;
  audio_chunk_t chunk = {0};
  esp_http_client_handle_t client = NULL;

  while (true) {
    audio_config_t cfg = {0};
    bool need_reconnect = false;
    audio_streamer_copy_config(&cfg, &need_reconnect);

    if (!audio_streamer_should_stream(&cfg)) {
      if (client) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        client = NULL;
      }
      if (s_queue) {
        xQueueReset(s_queue);
      }
      s_accum_frames = 0;
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
      continue;
    }

    if (client && need_reconnect) {
      esp_http_client_close(client);
      esp_http_client_cleanup(client);
      client = NULL;
      if (s_queue) {
        xQueueReset(s_queue);
      }
      s_accum_frames = 0;
    }

    if (!client) {
      esp_http_client_config_t http_cfg = {
          .url = cfg.upload_url,
          .method = HTTP_METHOD_POST,
          .timeout_ms = 5000,
      };
      client = esp_http_client_init(&http_cfg);
      if (!client) {
        ESP_LOGE(TAG, "Failed to init http client");
        vTaskDelay(pdMS_TO_TICKS(STREAM_RETRY_MS));
        continue;
      }
      esp_http_client_set_header(client, "Content-Type", "audio/wav");
      esp_http_client_set_header(client, "Transfer-Encoding", "chunked");

      esp_err_t err = esp_http_client_open(client, 0);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        client = NULL;
        vTaskDelay(pdMS_TO_TICKS(STREAM_RETRY_MS));
        continue;
      }

      uint8_t wav_header[44] = {0};
      audio_streamer_build_wav_header(wav_header, s_sample_rate);
      int written = esp_http_client_write(client, (const char *)wav_header,
                                           sizeof(wav_header));
      if (written <= 0) {
        ESP_LOGW(TAG, "Failed to send WAV header");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        client = NULL;
        vTaskDelay(pdMS_TO_TICKS(STREAM_RETRY_MS));
        continue;
      }
    }

    if (xQueueReceive(s_queue, &chunk, pdMS_TO_TICKS(500)) == pdTRUE) {
      int written =
          esp_http_client_write(client, (const char *)chunk.data, chunk.bytes);
      if (written <= 0) {
        ESP_LOGW(TAG, "HTTP write failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        client = NULL;
        vTaskDelay(pdMS_TO_TICKS(STREAM_RETRY_MS));
      }
    }
  }
}

void audio_streamer_init(void) {
  const mic_config *cfg = mic_get_config();
  if (cfg) {
    s_tap_size = cfg->tap_size;
    s_sample_rate = cfg->sampling_freq;
  } else {
    s_tap_size = MIC_DEFAULT_TAP_SIZE;
    s_sample_rate = MIC_SAMPLING_FREQUENCY;
  }

  s_cfg_mutex = xSemaphoreCreateMutex();
  s_queue = xQueueCreate(STREAM_QUEUE_LENGTH, sizeof(audio_chunk_t));
  audio_config_t cfg_init = audio_config_get();
  s_config = cfg_init;
  s_enabled = audio_streamer_should_stream(&s_config);
  s_need_reconnect = true;

  mic_add_tap_callback(audio_streamer_on_tap, NULL);

  xTaskCreatePinnedToCore(audio_streamer_task, "audio_stream",
                          STREAM_TASK_STACK, NULL, STREAM_TASK_PRIO, &s_task, 0);
}

void audio_streamer_apply_config(const audio_config_t *config) {
  if (!config || !s_cfg_mutex) {
    return;
  }

  if (xSemaphoreTake(s_cfg_mutex, portMAX_DELAY) == pdTRUE) {
    s_config = *config;
    s_enabled = audio_streamer_should_stream(&s_config);
    s_need_reconnect = true;
    xSemaphoreGive(s_cfg_mutex);
  }

  if (s_task) {
    xTaskNotifyGive(s_task);
  }
}
