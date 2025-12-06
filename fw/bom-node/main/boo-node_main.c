#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>

#include "impulse_detection.h"
#include "mic_input.h"
#include "ring_buffer.h"

#ifndef SAMPLING_FREQUENCY
#define SAMPLING_FREQUENCY 44000
#endif

#ifndef PRE_EVENT_MS
#define PRE_EVENT_MS 8
#endif

#ifndef POST_EVENT_MS
#define POST_EVENT_MS 16
#endif

static const char *TAG = "MAIN";

impulse_detector detL, detR;

int16_t arrL[TAP_COUNT * TAP_SIZE];
int16_t arrR[TAP_COUNT * TAP_SIZE];

bool detectedL;
bool detectedR;

void detection_task(void *arg) {

  impulse_detection_init(&detL);
  impulse_detection_init(&detR);

  while (1) {
    if (xSemaphoreTake(detection_semaphore, portMAX_DELAY) == pdTRUE) {

      mic_save_event(arrL, arrR);

      detectedL = impulse_run_detection(&detL);
      detectedR = false;
      if (!detectedL) {
        detectedR = impulse_run_detection(&detR);
      }
    }
  }
}

void app_main(void) {

  mic_config mic_cfg = {
      .sampling_freq = SAMPLING_FREQUENCY,
      .pre_event_ms = PRE_EVENT_MS,
      .post_event_ms = POST_EVENT_MS,
      .num_taps = TAP_COUNT,
      .tap_size = TAP_SIZE,
  };

  mic_init(&mic_cfg);

  xTaskCreatePinnedToCore(mic_reader_task, "mic_reader", 8192, NULL, 5, NULL,
                          0);

  xTaskCreatePinnedToCore(detection_task, "detector", 8192 * 2, NULL, 4, NULL,
                          1);

  vTaskDelay(pdMS_TO_TICKS(1000));

  int wanted_window_start =
      TAP_COUNT * TAP_SIZE / 2 - PRE_EVENT_MS * SAMPLING_FREQUENCY / 1000;
  ESP_LOGI(TAG, "wws - %d", wanted_window_start);

  int wanted_window_length =
      (PRE_EVENT_MS + POST_EVENT_MS) * SAMPLING_FREQUENCY / 1000;
  ESP_LOGI(TAG, "wwl - %d", wanted_window_length);

  while (1) {
    if (detectedL || detectedR) {
      detectedL = false;
      detectedR = false;

      ESP_LOGI(TAG, ">>> IMPULSE DETECTED <<<");

      for (int i = 0; i < wanted_window_length; i++) {
        printf("%d ", arrL[wanted_window_start + i]);
      }
      printf("\n");
      for (int i = 0; i < wanted_window_length; i++) {
        printf("%d ", arrR[wanted_window_start + i]);
      }
      printf("\n");

      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}
