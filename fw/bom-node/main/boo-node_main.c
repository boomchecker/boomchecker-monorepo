#include "esp_err.h"
#include "esp_log.h"
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

static const char *TAG = "MAIN";

void app_main(void) {
  esp_err_t err = middleware_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Middleware init failed: %s", esp_err_to_name(err));
  }

#ifdef CONFIG_OTA_ENABLE
  err = ota_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA init failed: %s", esp_err_to_name(err));
  } else {
    ota_check_for_update();
  }
#endif

  mic_init_default();

  while (1) {
    vTaskDelay(1);
  }
}
