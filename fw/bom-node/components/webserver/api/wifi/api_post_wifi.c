#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "error_handler.h"
#include "esp_err.h"
#include "esp_log.h"
#include "handler.h" // For route_entry_t and route_request
#include "wifi.h"
#include "wifi_config.h"
#include "wifi_api.h"

static const char* TAG = "POST_WIFI";

// Prototypes for the handler functions
esp_err_t post_wifi_connect(httpd_req_t* req);
esp_err_t post_wifi_ap(httpd_req_t* req);

// Table of routes
static const route_entry_t route_table[] = {{"^/api/v1/wifi/connect/?$", post_wifi_connect},
                                            {"^/api/v1/wifi/ap/?$", post_wifi_ap}};

// Main handler for POST wifi/* requests
esp_err_t api_post_wifi(httpd_req_t* req) {
    ESP_LOGI(TAG, "Received POST request: %s", req->uri);

    return route_request(req, route_table, sizeof(route_table) / sizeof(route_table[0]));
}

//////////////////////////////
// Implementations of handlers
//////////////////////////////

/**
 * POST /api/v1/wifi/connect
 * @summary Update WiFi configuration
 * @tag Wi-Fi
 * @bodyDescription Update the WiFi configuration with the provided SSID and password.
 * @bodyContent {WifiConfig} application/json
 * @bodyRequired
 * @response 200 - WiFi config updated
 * @response 500 - Internal error
 */
/**
 * @brief Handles POST requests to update Wi-Fi configuration.
 * @param req The HTTP request object.
 * @return ESP_OK on success, or an error code on failure.
 * 
 *  * Request body **must** be JSON:
 * @code{.json}
 * {
 *   "ssid"     : "<access‑point‑name>",
 *   "password" : "<passphrase>"
 * }
 * @endcode.
 *
 * ### JSON‑error schema
 * ```json
 * {
 *   "httpStatus" : 400,
 *   "tag"        : "wifi",
 *   "code"       : "WIFI_MISSING_FIELDS",
 *   "message"    : "Missing required fields"
 * }
 * ```
 *
 * | HTTP | Code               | Description                              |
 * |------|--------------------|------------------------------------------|
 * | 400  | **INVALID_BODY**        | Body not received or length ≤ 0.        |
 * | 400  | **WIFI_INVALID_JSON**   | Body is not valid JSON.                |
 * | 400  | **WIFI_MISSING_FIELDS** | Either *ssid* or *password* is absent. |
 * | 500  | **WIFI_CONNECT_FAIL**   | ESP32 failed to join the network.      |
 *
 * @note Uses a fixed 256‑byte stack buffer.  Increase if larger payloads
 *       are expected, or stream the body in chunks.
 */
static esp_err_t read_json_body(httpd_req_t* req, char* buf, size_t buf_len) {
    if (req->content_len <= 0 || req->content_len >= (int)buf_len) {
        return ESP_ERR_INVALID_SIZE;
    }

    int len = httpd_req_recv(req, buf, buf_len - 1);
    if (len <= 0) {
        return ESP_FAIL;
    }
    buf[len] = '\0';
    return ESP_OK;
}

esp_err_t post_wifi_connect(httpd_req_t* req) {
    ESP_LOGI(TAG, "Handling WiFi connect");

    char buf[256];
    if (read_json_body(req, buf, sizeof buf) != ESP_OK) {
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Failed to receive request body");
    }

    /* ---------- parse JSON ---------- */
    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Invalid JSON format");
    }

    const cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
    const cJSON* password = cJSON_GetObjectItem(root, "password");
    if (!cJSON_IsString(ssid) || !cJSON_IsString(password)) {
        cJSON_Delete(root);
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Missing required fields");
    }

    /* ---------- Wi‑Fi connect ---------- */
    if (wifi_api_connect_and_store(ssid->valuestring, password->valuestring) != ESP_OK) {
        cJSON_Delete(root);
        return send_json_error(req, TAG, WEBERR_INTERNAL_ERR, "Failed to connect to WiFi");
    }

    cJSON_Delete(root);

    /* ---------- success ---------- */
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/**
 * POST /api/v1/wifi/ap
 * @summary Toggle AP mode
 * @tag Wi-Fi
 * @bodyDescription Enable/disable AP and optionally update SSID.
 * @bodyContent {WifiApConfig} application/json
 * @bodyRequired
 * @response 200 - AP config updated
 * @response 500 - Internal error
 */
esp_err_t post_wifi_ap(httpd_req_t* req) {
    ESP_LOGI(TAG, "Handling WiFi AP config");

    char buf[256];
    if (read_json_body(req, buf, sizeof buf) != ESP_OK) {
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Failed to receive request body");
    }

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Invalid JSON format");
    }

    const cJSON* enabled = cJSON_GetObjectItem(root, "enabled");
    const cJSON* ssid = cJSON_GetObjectItem(root, "ssid");
    if (!cJSON_IsBool(enabled)) {
        cJSON_Delete(root);
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Missing required fields");
    }

    bool ap_enabled = cJSON_IsTrue(enabled);
    const char* ap_ssid = NULL;
    if (cJSON_IsString(ssid) && ssid->valuestring[0] != '\0') {
        ap_ssid = ssid->valuestring;
    }

    if (ap_enabled && !ap_ssid) {
        cJSON_Delete(root);
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "SSID required when enabling AP");
    }

    if (!ap_ssid) {
        ap_ssid = get_ap_ssid();
    }

    if (wifi_set_ap_config(ap_enabled, ap_ssid) != ESP_OK) {
        cJSON_Delete(root);
        return send_json_error(req, TAG, WEBERR_INTERNAL_ERR, "Failed to update AP config");
    }

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}
