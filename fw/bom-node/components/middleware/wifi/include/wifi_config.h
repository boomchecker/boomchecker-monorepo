#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "wifi_types.h"

void set_wifi_mode(wifi_mode_t mode);
wifi_mode_t get_wifi_mode(void);

void set_wifi_connected(bool connected);
bool is_wifi_connected(void);

void set_wifi_configured(bool configured);
bool is_wifi_configured(void);

bool is_wifi_credentials_set(void);
wifi_credentials_t get_wifi_credentials(void);

esp_err_t wifi_store_credentials(const char* ssid, const char* password);

void set_ap_enabled(bool enabled);
bool is_ap_enabled(void);

void set_ap_ssid(const char* ssid);
const char* get_ap_ssid(void);

esp_err_t wifi_store_ap_config(bool enabled, const char* ssid);
