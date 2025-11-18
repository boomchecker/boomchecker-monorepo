#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>

#include "mic_input.h"
#include "ring_buffer.h"

#define SAMPLING_FREQUENCY 24000
#define PRE_EVENT_MS 10
#define POST_EVENT_MS 40

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
  int16_t arrL[duration_n];
  int16_t arrR[duration_n];

  while (1) {
    mic_save_event(arrL, arrR);

    for (int i = 0; i < duration_n; i++) {
      printf("%d %d\n", arrL[i], arrR[i]);
    }
    printf("---\n");

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
