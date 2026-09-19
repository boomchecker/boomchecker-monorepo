#include "spiffs_init.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#define BASE_PATH "/storage"
#define TAG       "SPIFFS_INIT"

void init_spiffs_static(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS for static file handler");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = BASE_PATH,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    if (esp_spiffs_info(conf.partition_label, &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS size: total=%d, used=%d", total, used);
    } else {
        ESP_LOGW(TAG, "Failed to get SPIFFS info");
    }
}
