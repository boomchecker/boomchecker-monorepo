#include "wifi_config.h"

#include "sdkconfig.h"
#include <string.h>

static wifi_mode_t s_wifi_mode = WIFI_MODE_NULL;
static bool s_wifi_connected = false;
static bool s_wifi_configured = false;
static wifi_credentials_t s_wifi_credentials;
static bool s_wifi_credentials_initialized = false;

static void wifi_init_credentials(void)
{
    if (s_wifi_credentials_initialized)
    {
        return;
    }

    memset(&s_wifi_credentials, 0, sizeof(s_wifi_credentials));
    strncpy(s_wifi_credentials.ssid, CONFIG_MIDDLEWARE_WIFI_SSID,
            sizeof(s_wifi_credentials.ssid) - 1);
    strncpy(s_wifi_credentials.password, CONFIG_MIDDLEWARE_WIFI_PASSWORD,
            sizeof(s_wifi_credentials.password) - 1);
    s_wifi_credentials_initialized = true;
    s_wifi_configured = s_wifi_credentials.ssid[0] != '\0';
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
