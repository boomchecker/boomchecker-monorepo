#include "audio_config.h"

#include <string.h>
#include "nvs.h"

#define AUDIO_NVS_NAMESPACE "audio"
#define AUDIO_NVS_MODE      "mode"
#define AUDIO_NVS_URL       "upload_url"
#define AUDIO_NVS_ENABLED   "enabled"

static bool s_audio_config_initialized = false;
static audio_config_t s_audio_config = {0};

static void audio_config_load(void)
{
    if (s_audio_config_initialized)
    {
        return;
    }

    memset(&s_audio_config, 0, sizeof(s_audio_config));
    strncpy(s_audio_config.mode, "disabled", sizeof(s_audio_config.mode) - 1);
    s_audio_config.upload_url[0] = '\0';
    s_audio_config.enabled = false;

    nvs_handle_t handle;
    if (nvs_open(AUDIO_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK)
    {
        size_t mode_len = sizeof(s_audio_config.mode);
        if (nvs_get_str(handle, AUDIO_NVS_MODE, s_audio_config.mode, &mode_len) != ESP_OK)
        {
            strncpy(s_audio_config.mode, "disabled", sizeof(s_audio_config.mode) - 1);
        }

        size_t url_len = sizeof(s_audio_config.upload_url);
        if (nvs_get_str(handle, AUDIO_NVS_URL, s_audio_config.upload_url, &url_len) != ESP_OK)
        {
            s_audio_config.upload_url[0] = '\0';
        }

        uint8_t enabled = 0;
        if (nvs_get_u8(handle, AUDIO_NVS_ENABLED, &enabled) == ESP_OK)
        {
            s_audio_config.enabled = enabled != 0;
        }

        nvs_close(handle);
    }

    s_audio_config_initialized = true;
}

audio_config_t audio_config_get(void)
{
    audio_config_load();
    return s_audio_config;
}

esp_err_t audio_config_set(const audio_config_t* config)
{
    if (!config)
    {
        return ESP_ERR_INVALID_ARG;
    }

    audio_config_load();
    strncpy(s_audio_config.mode, config->mode, sizeof(s_audio_config.mode) - 1);
    s_audio_config.mode[sizeof(s_audio_config.mode) - 1] = '\0';
    strncpy(s_audio_config.upload_url, config->upload_url,
            sizeof(s_audio_config.upload_url) - 1);
    s_audio_config.upload_url[sizeof(s_audio_config.upload_url) - 1] = '\0';
    s_audio_config.enabled = config->enabled;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(AUDIO_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_str(handle, AUDIO_NVS_MODE, s_audio_config.mode);
    if (err == ESP_OK)
    {
        err = nvs_set_str(handle, AUDIO_NVS_URL, s_audio_config.upload_url);
    }
    if (err == ESP_OK)
    {
        err = nvs_set_u8(handle, AUDIO_NVS_ENABLED, s_audio_config.enabled ? 1 : 0);
    }
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

bool audio_config_is_configured(void)
{
    audio_config_load();
    return s_audio_config.upload_url[0] != '\0';
}
