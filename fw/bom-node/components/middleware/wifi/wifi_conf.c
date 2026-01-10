#include "wifi_config.h"

#include "sdkconfig.h"
#include <string.h>

#include "nvs.h"

#define WIFI_NVS_NAMESPACE "wifi"
#define WIFI_NVS_STA_SSID  "sta_ssid"
#define WIFI_NVS_STA_PASS  "sta_pass"
#define WIFI_NVS_AP_SSID   "ap_ssid"
#define WIFI_NVS_AP_EN     "ap_enabled"

static wifi_mode_t s_wifi_mode = WIFI_MODE_NULL;
static bool s_wifi_connected = false;
static bool s_wifi_configured = false;
static wifi_credentials_t s_wifi_credentials;
static bool s_wifi_credentials_initialized = false;
static bool s_ap_config_initialized = false;
static bool s_ap_enabled = true;
static char s_ap_ssid[32];

static esp_err_t wifi_nvs_open(nvs_handle_t* handle) {
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    return nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, handle);
}

static void wifi_init_credentials(void)
{
    if (s_wifi_credentials_initialized)
    {
        return;
    }

    memset(&s_wifi_credentials, 0, sizeof(s_wifi_credentials));

    nvs_handle_t handle;
    if (wifi_nvs_open(&handle) == ESP_OK)
    {
        size_t ssid_len = sizeof(s_wifi_credentials.ssid);
        size_t pass_len = sizeof(s_wifi_credentials.password);

        if (nvs_get_str(handle, WIFI_NVS_STA_SSID, s_wifi_credentials.ssid, &ssid_len)
            != ESP_OK)
        {
            s_wifi_credentials.ssid[0] = '\0';
        }

        if (nvs_get_str(handle, WIFI_NVS_STA_PASS, s_wifi_credentials.password, &pass_len)
            != ESP_OK)
        {
            s_wifi_credentials.password[0] = '\0';
        }

        nvs_close(handle);
    }

    if (s_wifi_credentials.ssid[0] == '\0')
    {
        strncpy(s_wifi_credentials.ssid, CONFIG_MIDDLEWARE_WIFI_SSID,
                sizeof(s_wifi_credentials.ssid) - 1);
        strncpy(s_wifi_credentials.password, CONFIG_MIDDLEWARE_WIFI_PASSWORD,
                sizeof(s_wifi_credentials.password) - 1);
    }

    s_wifi_credentials_initialized = true;
    s_wifi_configured = s_wifi_credentials.ssid[0] != '\0';
}

static void wifi_init_ap_config(void)
{
    if (s_ap_config_initialized)
    {
        return;
    }

    memset(s_ap_ssid, 0, sizeof(s_ap_ssid));
    strncpy(s_ap_ssid, "FELIX-MB", sizeof(s_ap_ssid) - 1);
    s_ap_enabled = true;

    nvs_handle_t handle;
    if (wifi_nvs_open(&handle) == ESP_OK)
    {
        size_t ssid_len = sizeof(s_ap_ssid);
        if (nvs_get_str(handle, WIFI_NVS_AP_SSID, s_ap_ssid, &ssid_len) != ESP_OK)
        {
            s_ap_ssid[0] = '\0';
        }

        uint8_t ap_enabled = 0;
        if (nvs_get_u8(handle, WIFI_NVS_AP_EN, &ap_enabled) == ESP_OK)
        {
            s_ap_enabled = ap_enabled != 0;
        }

        nvs_close(handle);
    }

    if (s_ap_ssid[0] == '\0')
    {
        strncpy(s_ap_ssid, "FELIX-MB", sizeof(s_ap_ssid) - 1);
    }

    s_ap_config_initialized = true;
}

void set_wifi_mode(wifi_mode_t mode)
{
    s_wifi_mode = mode;
}

wifi_mode_t get_wifi_mode(void)
{
    return s_wifi_mode;
}

void set_wifi_connected(bool connected)
{
    s_wifi_connected = connected;
}

bool is_wifi_connected(void)
{
    return s_wifi_connected;
}

void set_wifi_configured(bool configured)
{
    s_wifi_configured = configured;
}

bool is_wifi_configured(void)
{
    return s_wifi_configured;
}

bool is_wifi_credentials_set(void)
{
    wifi_init_credentials();
    return s_wifi_credentials.ssid[0] != '\0';
}

wifi_credentials_t get_wifi_credentials(void)
{
    wifi_init_credentials();
    return s_wifi_credentials;
}

esp_err_t wifi_store_credentials(const char* ssid, const char* password)
{
    if (!ssid || !password)
    {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_init_credentials();

    strncpy(s_wifi_credentials.ssid, ssid, sizeof(s_wifi_credentials.ssid) - 1);
    s_wifi_credentials.ssid[sizeof(s_wifi_credentials.ssid) - 1] = '\0';
    strncpy(s_wifi_credentials.password, password, sizeof(s_wifi_credentials.password) - 1);
    s_wifi_credentials.password[sizeof(s_wifi_credentials.password) - 1] = '\0';
    s_wifi_configured = s_wifi_credentials.ssid[0] != '\0';

    nvs_handle_t handle;
    esp_err_t err = wifi_nvs_open(&handle);
    if (err != ESP_OK)
    {
        return err;
    }

    err = nvs_set_str(handle, WIFI_NVS_STA_SSID, s_wifi_credentials.ssid);
    if (err == ESP_OK)
    {
        err = nvs_set_str(handle, WIFI_NVS_STA_PASS, s_wifi_credentials.password);
    }
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

void set_ap_enabled(bool enabled)
{
    s_ap_enabled = enabled;
}

bool is_ap_enabled(void)
{
    wifi_init_ap_config();
    return s_ap_enabled;
}

void set_ap_ssid(const char* ssid)
{
    if (!ssid)
    {
        return;
    }

    strncpy(s_ap_ssid, ssid, sizeof(s_ap_ssid) - 1);
    s_ap_ssid[sizeof(s_ap_ssid) - 1] = '\0';
}

const char* get_ap_ssid(void)
{
    wifi_init_ap_config();
    return s_ap_ssid;
}

esp_err_t wifi_store_ap_config(bool enabled, const char* ssid)
{
    wifi_init_ap_config();
    if (ssid && ssid[0] != '\0')
    {
        set_ap_ssid(ssid);
    }
    set_ap_enabled(enabled);

    nvs_handle_t handle;
    esp_err_t err = wifi_nvs_open(&handle);
    if (err != ESP_OK)
    {
        return err;
    }

    uint8_t ap_enabled = enabled ? 1 : 0;
    err = nvs_set_u8(handle, WIFI_NVS_AP_EN, ap_enabled);
    if (err == ESP_OK)
    {
        err = nvs_set_str(handle, WIFI_NVS_AP_SSID, s_ap_ssid);
    }
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}
