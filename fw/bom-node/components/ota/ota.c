#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "otadrive_esp.h"

#include "ota.h"

static const char *TAG = "ota";

esp_err_t ota_init(void) {
  if (CONFIG_OTA_API_KEY[0] == '\0') {
    ESP_LOGW(TAG, "OTAdrive API key is empty");
    return ESP_ERR_INVALID_ARG;
  }

  otadrive_setInfo((char *)CONFIG_OTA_API_KEY,
                   (char *)CONFIG_OTA_CURRENT_VERSION);
  return ESP_OK;
}
