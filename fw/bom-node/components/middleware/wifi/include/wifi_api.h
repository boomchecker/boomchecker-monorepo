#pragma once

#include "esp_err.h"
#include "wifi_types.h"

esp_err_t wifi_api_scan(wifi_scan_result_t *result);
esp_err_t wifi_api_connect_and_store(const char *ssid, const char *password);
