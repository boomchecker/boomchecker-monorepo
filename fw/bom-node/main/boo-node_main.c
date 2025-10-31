#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#include "mic_input.h"
#include "ring_buffer.h"

#define SAMPLING_FREQUENCY 16000
#define PRE_EVENT_MS 10
#define POST_EVENT_MS 40

static const char *TAG = "example";

void app_main(void) {

  mic_config mic_cfg = {
      .sampling_freq = SAMPLING_FREQUENCY,
      .pre_event_ms = PRE_EVENT_MS,
      .post_event_ms = POST_EVENT_MS,
  };

  mic_init(&mic_cfg);
  mic_start_reading();

  int duration_n = (PRE_EVENT_MS + POST_EVENT_MS) * SAMPLING_FREQUENCY / 1000;
  int *arrL = (int *)malloc(duration_n * sizeof(int));
  int *arrR = (int *)malloc(duration_n * sizeof(int));

  while (1) {

    mic_save_event(arrL, arrR);

    for (int i = 0; i < duration_n; i++) {
      printf("   %d   %d\n", arrL[i], arrR[i]);
    }

    vTaskDelay(1000);
  }

  free(arrL);
  free(arrR);
}
