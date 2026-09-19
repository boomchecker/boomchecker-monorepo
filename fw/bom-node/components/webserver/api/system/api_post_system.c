#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "api_post_system.h"
#include "handler.h"
#include "slre.h"

static const char* TAG = "POST_SYSTEM";

// Definition of handlers
static esp_err_t post_system_reboot(httpd_req_t* req);

// Table of routes
static const route_entry_t route_table[] = {{"^/api/v1/system/reboot/?$", post_system_reboot}};

// Main handler for POST system/* requests
esp_err_t api_post_system(httpd_req_t* req) {
    ESP_LOGI(TAG, "Received POST request: %s", req->uri);
    return route_request(req, route_table, sizeof(route_table) / sizeof(route_entry_t));
}

static esp_err_t post_system_reboot(httpd_req_t* req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"rebooting\"}");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return ESP_OK;
}
