// ===========================
// File: wifi.c
// ===========================

#include "wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/inet.h"
#include "wifi_config.h"
#include "wifi_types.h"
#include <string.h>

#define TAG "wifi"

static int retry_count = 0;
static bool s_got_ip = false;
static SemaphoreHandle_t s_connect_sema = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id,
                               void *data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    s_got_ip = false;
    if (retry_count++ < 5) {
      esp_wifi_connect();
    } else {
      if (s_connect_sema) {
        xSemaphoreGive(s_connect_sema);
      }
    }
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    s_got_ip = true;
    if (s_connect_sema) {
      xSemaphoreGive(s_connect_sema);
    }
  }
}

void start_apsta_mode(void) {
  // Customize AP IP address, gateway and netmask
  esp_netif_ip_info_t ip_info;
  esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

  // Customize AP IP address, gateway and netmask
  IP4_ADDR(&ip_info.ip, 192, 168, 10, 10);
  IP4_ADDR(&ip_info.gw, 192, 168, 10, 10);
  IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

  esp_netif_dhcps_stop(ap_netif);
  esp_netif_set_ip_info(ap_netif, &ip_info);
  esp_netif_dhcps_start(ap_netif);

  // Init Wi-Fi
  esp_netif_create_default_wifi_sta(); // Default sta netif
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  wifi_config_t ap_config = {.ap = {.ssid = "FELIX-MB",
                                    .ssid_len = strlen("FELIX-MB"),
                                    .password = "12345678",
                                    .max_connection = 2,
                                    .authmode = WIFI_AUTH_WPA_WPA2_PSK}};

  if (strlen((const char *)ap_config.ap.password) == 0) {
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  esp_wifi_set_mode(WIFI_MODE_APSTA);
  esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config);
  esp_wifi_start();
  set_wifi_mode(WIFI_MODE_APSTA);
}

esp_err_t wifi_try_reconnect(void) {
  if (!s_connect_sema) {
    s_connect_sema = xSemaphoreCreateBinary();
  }

  retry_count = 0;
  s_got_ip = false;

  esp_wifi_disconnect();

  for (int i = 0; i < 5; i++) {
    esp_wifi_connect();

    if (xSemaphoreTake(s_connect_sema, pdMS_TO_TICKS(30000)) == pdTRUE) {
      if (s_got_ip) {
        ESP_LOGI(TAG, "Reconnected successfully.");
        set_wifi_connected(true);
        return ESP_OK;
      } else {
        ESP_LOGW(TAG, "Reconnection attempt %d failed.", i + 1);
      }
    } else {
      ESP_LOGE(TAG, "Reconnect attempt %d timeout.", i + 1);
    }
  }

  ESP_LOGE(TAG, "Reconnection failed after 5 attempts.");
  set_wifi_connected(false);
  return ESP_FAIL;
}

esp_err_t wifi_connect_with_credentials(const char *ssid,
                                        const char *password) {
  if (!ssid || !password) {
    return ESP_ERR_INVALID_ARG;
  }

  wifi_config_t cfg = {0};
  strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
  strncpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password) - 1);
  cfg.sta.ssid[sizeof(cfg.sta.ssid) - 1] = '\0';
  cfg.sta.password[sizeof(cfg.sta.password) - 1] = '\0';

  ESP_LOGI(TAG, "Switching to APSTA mode...");
  esp_wifi_set_mode(WIFI_MODE_APSTA);
  set_wifi_mode(WIFI_MODE_APSTA);

  ESP_LOGI(TAG, "Disconnecting if already connected...");
  esp_wifi_disconnect();
  vTaskDelay(pdMS_TO_TICKS(100));

  ESP_LOGI(TAG, "Setting new STA config...");
  esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg);

  set_wifi_configured(true);
  return wifi_try_reconnect();
}

esp_err_t wifi_scan_networks(wifi_scan_result_t *result) {
  if (!result) {
    return ESP_ERR_INVALID_ARG;
  }

  wifi_mode_t current_mode = get_wifi_mode();
  if (current_mode == WIFI_MODE_AP) {
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    set_wifi_mode(WIFI_MODE_APSTA);
  }

  wifi_scan_config_t cfg = {
      .show_hidden = false,
      .scan_type = WIFI_SCAN_TYPE_ACTIVE,
      .scan_time.active = {.min = 200, .max = 400},
  };

  esp_wifi_scan_start(&cfg, true);

  uint16_t count = 0;
  esp_wifi_scan_get_ap_num(&count);
  count = count > MAX_WIFI_SCAN_RESULTS ? MAX_WIFI_SCAN_RESULTS : count;

  result->count = count;
  return esp_wifi_scan_get_ap_records(&count, result->records);
}

void wifi_main_func(void) {
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                      wifi_event_handler, NULL, NULL);
  esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                      wifi_event_handler, NULL, NULL);

  start_apsta_mode();

  if (is_wifi_credentials_set()) {
    wifi_connect_with_credentials(get_wifi_credentials().ssid,
                                  get_wifi_credentials().password);
  }
}