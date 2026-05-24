#pragma once

#include "esp_err.h"
#include "wifi_types.h"

esp_err_t wifi_init(void);
void wifi_main_func(void);
void start_apsta_mode(void);
esp_err_t wifi_try_reconnect(void);
esp_err_t wifi_connect_with_credentials(const char *ssid, const char *password);
esp_err_t wifi_scan_networks(wifi_scan_result_t *result);
esp_err_t wifi_set_ap_config(bool enabled, const char *ssid);
