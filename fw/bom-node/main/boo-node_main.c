#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "example";

void app_main(void) {

  while (1) {

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
