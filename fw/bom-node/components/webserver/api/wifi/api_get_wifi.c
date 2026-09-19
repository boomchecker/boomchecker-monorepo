#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"

#include "api_get_wifi.h"
#include "cJSON.h"
#include "handler.h"
#include "slre.h"
#include "wifi.h"
#include "wifi_config.h"
#include "wifi_api.h"

static const char* TAG = "GET_WIFI";

// Definition of handlers
esp_err_t get_wifi_scan(httpd_req_t* req);
esp_err_t get_wifi_status(httpd_req_t* req);

// Table of routes
static const route_entry_t route_table[] = {{"^/api/v1/wifi/scan/?$", get_wifi_scan},
                                            {"^/api/v1/wifi/status/?$", get_wifi_status}};

// Main handler for GET wifi/* requests
esp_err_t api_get_wifi(httpd_req_t* req) {
    ESP_LOGI(TAG, "Received GET request: %s", req->uri);

    return route_request(req, route_table, sizeof(route_table) / sizeof(route_entry_t));
}

//////////////////////////////
// Implementations of handlers
//////////////////////////////

/**
 * GET /api/v1/wifi/scan
 * @summary Scan for WiFi networks
 * @tag Wi-Fi
 * @response 200 - List of available networks
 * @response 500 - Internal error
 * @responseContent {WifiSearch} 200.application/json
 * @responseExample {WifiSearch200} 200.application/json.200
 */
/** 
 * @brief Handler for searching WiFi networks.
 * 
 * @param req HTTP request
 * @return ESP_OK on success
 */
esp_err_t get_wifi_scan(httpd_req_t* req) {
    // Create JSON response
    cJSON* root = cJSON_CreateObject();
    cJSON* ssids = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "ssids", ssids);

    wifi_scan_result_t ssid_result;
    esp_err_t err = wifi_api_scan(&ssid_result);
    if (err != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    for (int i = 0; i < ssid_result.count; i++) {
        wifi_ap_record_t record = ssid_result.records[i];
        cJSON_AddItemToArray(ssids, cJSON_CreateString((const char*)record.ssid));
    }

    // Convert JSON to string and send response
    const char* resp_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));

    // Clean up
    cJSON_Delete(root);
    free((void*)resp_str); // Important: cJSON_PrintUnformatted allocates memory

    return ESP_OK;
}

/**
 * GET /api/v1/wifi/status
 * @summary Get WiFi status
 * @tag Wi-Fi
 * @response 200 - WiFi status
 * @response 500 - Internal error
 */
esp_err_t get_wifi_status(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return httpd_resp_send_500(req);
    }

    wifi_credentials_t creds = get_wifi_credentials();
    cJSON_AddBoolToObject(root, "connected", is_wifi_connected());
    cJSON_AddBoolToObject(root, "configured", is_wifi_configured());
    cJSON_AddBoolToObject(root, "apEnabled", is_ap_enabled());
    cJSON_AddStringToObject(root, "ssid", creds.ssid);
    cJSON_AddStringToObject(root, "apSsid", get_ap_ssid());

    const char* resp_str = cJSON_PrintUnformatted(root);
    if (!resp_str) {
        cJSON_Delete(root);
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));

    cJSON_Delete(root);
    free((void*)resp_str);
    return ESP_OK;
}
