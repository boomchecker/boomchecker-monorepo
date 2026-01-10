#pragma once

#include "esp_err.h"
#include <stdbool.h>

#define AUDIO_MODE_MAX_LEN 16
#define AUDIO_URL_MAX_LEN  128

typedef struct
{
    char mode[AUDIO_MODE_MAX_LEN];
    char upload_url[AUDIO_URL_MAX_LEN];
} audio_config_t;

audio_config_t audio_config_get(void);
esp_err_t audio_config_set(const audio_config_t* config);
bool audio_config_is_configured(void);
