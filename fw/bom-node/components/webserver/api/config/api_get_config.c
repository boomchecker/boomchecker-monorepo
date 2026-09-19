#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"

#include "api_get_config.h"
#include "cJSON.h"
#include "error_handler.h"
#include "handler.h"
#include "slre.h"
#include "audio_config.h"
#include "wifi.h"
#include "wifi_config.h"

static const char* TAG = "GET_CONFIG";

// Definition of handlers
esp_err_t get_config_status(httpd_req_t* req);

// Table of routes
static const route_entry_t route_table[] = {{"^/api/v1/config/?$", get_config_status}};

// Main handler for GET config/* requests
esp_err_t api_get_config(httpd_req_t* req) {
    ESP_LOGI(TAG, "Received GET request: %s", req->uri);

    return route_request(req, route_table, sizeof(route_table) / sizeof(route_entry_t));
}

//////////////////////////////
// Implementations of handlers
//////////////////////////////

/**
 * GET /api/v1/config
 * @summary Get device status
 * @tag Device
 * @response 200 - Device status
 * @response 500 - Internal error
 * @responseContent {ConfigStatus} 200.application/json
 * @responseExample {ConfigStatus200} 200.application/json.200
*/
esp_err_t get_config_status(httpd_req_t* req) {
    // Build JSON response using cJSON
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return send_json_error(req, TAG, WEBERR_INTERNAL_ERR, "Failed to allocate cJSON");
    }

    bool wifiConfigured = is_wifi_configured();
    bool wifiConnected = is_wifi_connected();
    bool apEnabled = is_ap_enabled();
    bool audioConfigured = audio_config_is_configured();

    bool isSetupDone = wifiConfigured;

    cJSON_AddBoolToObject(root, "isSetupDone", isSetupDone);
    cJSON_AddBoolToObject(root, "wifiConfigured", wifiConfigured);
    cJSON_AddBoolToObject(root, "wifiConnected", wifiConnected);
    cJSON_AddBoolToObject(root, "apEnabled", apEnabled);
    cJSON_AddBoolToObject(root, "audioConfigured", audioConfigured);
    cJSON_AddStringToObject(root, "deviceName", "BOM-Node");

    const char* resp_str = cJSON_PrintUnformatted(root);
    if (!resp_str) {
        ESP_LOGE(TAG, "Failed to allocate memory for JSON response");
        cJSON_Delete(root);
        return send_json_error(req, TAG, WEBERR_INTERNAL_ERR,
                               "Failed to allocate memory for JSON response");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    cJSON_Delete(root);
    free((void*)resp_str);
    return ESP_OK;
}
