#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "error_handler.h"
#include "esp_err.h"
#include "esp_log.h"
#include "handler.h"

#include "api_post_audio.h"
#include "audio_config.h"
#include "slre.h"

static const char* TAG = "POST_AUDIO";

// Definition of handlers
esp_err_t post_audio_config(httpd_req_t* req);

// Table of routes
static const route_entry_t route_table[] = {{"^/api/v1/audio/?$", post_audio_config}};

// Main handler for POST audio/* requests
esp_err_t api_post_audio(httpd_req_t* req) {
    ESP_LOGI(TAG, "Received POST request: %s", req->uri);

    return route_request(req, route_table, sizeof(route_table) / sizeof(route_entry_t));
}

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

/**
 * POST /api/v1/audio
 * @summary Update audio configuration
 * @tag Audio
 * @bodyDescription Update the audio mode and upload URL.
 * @bodyContent {AudioConfig} application/json
 * @bodyRequired
 * @response 200 - Audio config updated
 * @response 500 - Internal error
 */
esp_err_t post_audio_config(httpd_req_t* req) {
    ESP_LOGI(TAG, "Handling audio config");

    char buf[256];
    if (read_json_body(req, buf, sizeof buf) != ESP_OK) {
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Failed to receive request body");
    }

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Invalid JSON format");
    }

    const cJSON* mode = cJSON_GetObjectItem(root, "mode");
    const cJSON* upload_url = cJSON_GetObjectItem(root, "uploadUrl");
    if (!cJSON_IsString(mode) || !cJSON_IsString(upload_url)) {
        cJSON_Delete(root);
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Missing required fields");
    }

    audio_config_t config = {0};
    strncpy(config.mode, mode->valuestring, sizeof(config.mode) - 1);
    strncpy(config.upload_url, upload_url->valuestring, sizeof(config.upload_url) - 1);

    if (audio_config_set(&config) != ESP_OK) {
        cJSON_Delete(root);
        return send_json_error(req, TAG, WEBERR_INTERNAL_ERR, "Failed to store audio config");
    }

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}
