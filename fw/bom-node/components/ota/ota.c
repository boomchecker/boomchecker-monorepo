#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "otadrive_esp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ota.h"

static const char *TAG = "ota";
static TaskHandle_t s_ota_check_task = NULL;

esp_err_t ota_init(void) {
  if (CONFIG_OTA_API_KEY[0] == '\0') {
    ESP_LOGW(TAG, "OTAdrive API key is empty");
    return ESP_ERR_INVALID_ARG;
  }

  otadrive_setInfo((char *)CONFIG_OTA_API_KEY,
                   (char *)CONFIG_OTA_CURRENT_VERSION);
  return ESP_OK;
}

static void ota_check_task(void *arg) {
  otadrive_result r = otadrive_updateFirmwareInfo();

  switch (r.code) {
  case OTADRIVE_NewFirmwareExists:
    ESP_LOGI(TAG, "Update available: %s (%ld bytes), current %s", r.version,
             (long)r.size, otadrive_currentversion());
    break;
  case OTADRIVE_AlreadyUpToDate:
    ESP_LOGI(TAG, "Firmware is up to date (%s)", otadrive_currentversion());
    break;
  case OTADRIVE_DeviceUnauthorized:
    ESP_LOGE(TAG, "Device unauthorized");
    break;
  case OTADRIVE_NoFirmwareExists:
    ESP_LOGW(TAG, "No firmware exists on server");
    break;
  default:
    ESP_LOGE(TAG, "Failed to check firmware (%d)", r.code);
    break;
  }

  s_ota_check_task = NULL;
  vTaskDelete(NULL);
}

esp_err_t ota_check_for_update(void) {
  if (s_ota_check_task != NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  if (xTaskCreate(ota_check_task, "ota_check", 8192, NULL, 5,
                  &s_ota_check_task) != pdPASS) {
    s_ota_check_task = NULL;
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}
