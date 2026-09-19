#include "wifi.h"
#include "wifi_config.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

esp_err_t wifi_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        return err;
    }

    wifi_main_func();
    return ESP_OK;
}

esp_err_t wifi_api_scan(wifi_scan_result_t *result)
{
    return wifi_scan_networks(result);
}

esp_err_t wifi_api_connect_and_store(const char *ssid, const char *password)
{
    esp_err_t err = wifi_connect_with_credentials(ssid, password);
    if (err != ESP_OK)
    {
        return err;
    }

    return wifi_store_credentials(ssid, password);
}
