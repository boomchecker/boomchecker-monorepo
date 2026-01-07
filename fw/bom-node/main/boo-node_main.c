#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "middleware.h"

#include "impulse_detection.h"
#include "mic_input.h"
#include "ota.h"
#include "ring_buffer.h"

#ifndef SAMPLING_FREQUENCY
#define SAMPLING_FREQUENCY 44100
#endif

#ifndef PRE_EVENT_MS
#define PRE_EVENT_MS 10
#endif

#ifndef POST_EVENT_MS
#define POST_EVENT_MS 10
#endif

static const char *TAG = "MAIN";

impulse_detector detL, detR;
bool detection_request;

int16_t arrL[TAP_COUNT * TAP_SIZE];
int16_t arrR[TAP_COUNT * TAP_SIZE];

void app_main(void) {
  esp_err_t err = middleware_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Middleware init failed: %s", esp_err_to_name(err));
  }

  err = ota_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA init failed: %s", esp_err_to_name(err));
  } else {
    ota_check_for_update();
  }

  mic_config mic_cfg = {
      .sampling_freq = SAMPLING_FREQUENCY,
      .pre_event_ms = PRE_EVENT_MS,
      .post_event_ms = POST_EVENT_MS,
      .num_taps = TAP_COUNT,
      .tap_size = TAP_SIZE,
  };

  mic_init(&mic_cfg);

  /*int wanted_window_start =
      ((TAP_COUNT * TAP_SIZE) / 2) - (PRE_EVENT_MS * SAMPLING_FREQUENCY / 1000);
  ESP_LOGI(TAG, "wws - %d", wanted_window_start);

  int wanted_window_length =
      (PRE_EVENT_MS + POST_EVENT_MS) * SAMPLING_FREQUENCY / 1000;
  ESP_LOGI(TAG, "wwl - %d", wanted_window_length);

  impulse_detection_init(&detL);
  impulse_detection_init(&detR);

  xTaskCreatePinnedToCore(mic_reader_task, "mic_reader", 8192, NULL, 5, NULL,
                          0);

  bool detectedL;
  bool detectedR;
  detection_request = false;

  vTaskDelay(pdMS_TO_TICKS(200));
  ESP_LOGI(TAG, "Initialization finished");*/

  while (1) {
    vTaskDelay(1);
    /*if (detection_request) {

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
    }*/
  }
}
