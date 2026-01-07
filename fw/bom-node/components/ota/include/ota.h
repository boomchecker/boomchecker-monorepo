#pragma once

#include "esp_err.h"

esp_err_t ota_init(void);
esp_err_t ota_check_for_update(void);
