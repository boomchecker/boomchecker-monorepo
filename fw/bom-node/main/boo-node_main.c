#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>

#include "impulse_detection.h"
#include "mic_input.h"
#include "ring_buffer.h"

#define SAMPLING_FREQUENCY 20000
#define PRE_EVENT_MS 5
#define POST_EVENT_MS 20

static const char *TAG = "MAIN";

void app_main(void) {

  mic_config mic_cfg = {
      .sampling_freq = SAMPLING_FREQUENCY,
      .pre_event_ms = PRE_EVENT_MS,
      .post_event_ms = POST_EVENT_MS,
  };

  mic_init(&mic_cfg);
  xTaskCreatePinnedToCore(mic_reader_task, "mic_reader", 8192, NULL, 5, NULL,
                          0);
  vTaskDelay(pdMS_TO_TICKS(1000));

  int duration_n = (PRE_EVENT_MS + POST_EVENT_MS) * SAMPLING_FREQUENCY / 1000;

  impulse_detection_init(NULL);

  int16_t arrL[duration_n];
  int16_t arrR[duration_n];

  while (1) {

    if (detection_request == true) {
      mic_save_event(arrL, arrR);
      bool detectedL = impulse_detect(arrL, duration_n);
      bool detectedR = impulse_detect(arrR, duration_n);

      if (detectedL || detectedR) {
        ESP_LOGI(TAG, ">>> DETEKCE IMPULZU <<<");

        for (int i = 0; i < duration_n; i++) {
          printf("%d ", arrL[i]);
        }
        printf("\n");

        for (int i = 0; i < duration_n; i++) {
          printf("%d ", arrR[i]);
        }
        printf("\n");
      }
      detection_request = false;
    }
  }
}
