#include "detector.h"
#include "median_detection.h"
#include "mic_input.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static const char *TAG = "IMPULSE";

static impulse_detector detL;
static impulse_detector detR;
static bool detection_request = false;
static int16_t arrL[TAP_COUNT * TAP_SIZE];
static int16_t arrR[TAP_COUNT * TAP_SIZE];
static int wanted_window_start = 0;
static int wanted_window_length = 0;

static void impulse_detection_on_tap(const int16_t *tap_left,
                                     const int16_t *tap_right, void *ctx) {
  (void)ctx;
  impulse_add_tap(&detL, tap_left);
  impulse_add_tap(&detR, tap_right);
  detection_request = true;
}

static void impulse_detection_task(void *arg) {
  (void)arg;
  bool detectedL = false;
  bool detectedR = false;

  vTaskDelay(pdMS_TO_TICKS(200));
  ESP_LOGI(TAG, "Initialization finished");

  while (1) {
    vTaskDelay(1);
    if (detection_request) {
      detection_request = false;

      mic_save_event(arrL, arrR);

      detectedL = impulse_run_detection(&detL);
      detectedR = false;
      if (!detectedL) {
        detectedR = impulse_run_detection(&detR);
      }

      if (detectedL || detectedR) {
        ESP_LOGI(TAG, ">>> IMPULSE DETECTED <<<");
        detectedL = false;
        detectedR = false;

        if ((wanted_window_start >= 0) &&
            (wanted_window_start + wanted_window_length <=
             TAP_COUNT * TAP_SIZE)) {
          for (int i = 0; i < wanted_window_length; i++) {
            printf("%d ", arrL[wanted_window_start + i]);
          }
          printf("\n");
          for (int i = 0; i < wanted_window_length; i++) {
            printf("%d ", arrR[wanted_window_start + i]);
          }
          printf("\n");
        } else {
          ESP_LOGE(
              TAG, "Window out of bounds: start=%d, length=%d, array size=%d",
              wanted_window_start, wanted_window_length, TAP_COUNT * TAP_SIZE);
        }
      }
    }
  }
}

void impulse_detector_start(void) {
  const mic_config *cfg = mic_get_config();
  if (cfg == NULL) {
    ESP_LOGE(TAG, "mic_get_config failed; call mic_init first");
    return;
  }

  wanted_window_start = ((TAP_COUNT * TAP_SIZE) / 2) -
                        (cfg->pre_event_ms * cfg->sampling_freq / 1000);
  ESP_LOGI(TAG, "wws - %d", wanted_window_start);

  wanted_window_length =
      (cfg->pre_event_ms + cfg->post_event_ms) * cfg->sampling_freq / 1000;
  ESP_LOGI(TAG, "wwl - %d", wanted_window_length);

  impulse_detection_init(&detL);
  impulse_detection_init(&detR);

  detection_request = false;
  mic_set_tap_callback(impulse_detection_on_tap, NULL);
  mic_start();

  xTaskCreatePinnedToCore(impulse_detection_task, "impulse_detection", 8192,
                          NULL, 5, NULL, 0);
}
