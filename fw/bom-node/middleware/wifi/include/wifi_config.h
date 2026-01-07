#pragma once

#include <stdbool.h>
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
