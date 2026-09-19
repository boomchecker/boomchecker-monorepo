#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "error_handler.h"
#include "esp_err.h"
#include "esp_log.h"
#include "handler.h"

#include "api_post_audio.h"
#include "audio_config.h"
#include "audio_streamer.h"
#include "slre.h"

static const char* TAG = "POST_AUDIO";

// Definition of handlers
esp_err_t post_audio_stream_config(httpd_req_t* req);
esp_err_t post_audio_settings(httpd_req_t* req);

// Table of routes
static const route_entry_t route_table[] = {
    {"^/api/v1/audio/stream/?$", post_audio_stream_config},
    {"^/api/v1/audio/settings/?$", post_audio_settings},
};

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
 * POST /api/v1/audio/stream
 * @summary Update audio stream configuration
 * @tag Audio
 * @bodyDescription Update the audio mode, enabled flag, and upload URL.
 * @bodyContent {AudioConfig} application/json
 * @bodyRequired
 * @response 200 - Audio config updated
 * @response 500 - Internal error
 */
esp_err_t post_audio_stream_config(httpd_req_t* req) {
    ESP_LOGI(TAG, "Handling audio stream config");

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
    const cJSON* enabled = cJSON_GetObjectItem(root, "enabled");
    if (!cJSON_IsString(mode) || !cJSON_IsString(upload_url)) {
        cJSON_Delete(root);
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Missing required fields");
    }
    if (enabled) {
        if (!cJSON_IsBool(enabled)) {
            cJSON_Delete(root);
            return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Invalid enabled field");
        }
    }

    audio_config_t config = audio_config_get();
    strncpy(config.mode, mode->valuestring, sizeof(config.mode) - 1);
    strncpy(config.upload_url, upload_url->valuestring, sizeof(config.upload_url) - 1);
    config.enabled = enabled ? cJSON_IsTrue(enabled) : false;

    if (audio_config_set(&config) != ESP_OK) {
        cJSON_Delete(root);
        return send_json_error(req, TAG, WEBERR_INTERNAL_ERR, "Failed to store audio config");
    }
    audio_streamer_apply_config(&config);

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/**
 * POST /api/v1/audio/settings
 * @summary Update audio capture settings
 * @tag Audio
 * @bodyDescription Update the audio sampling rate.
 * @bodyContent {AudioSettings} application/json
 * @bodyRequired
 * @response 200 - Audio settings updated
 * @response 500 - Internal error
 */
esp_err_t post_audio_settings(httpd_req_t* req) {
    ESP_LOGI(TAG, "Handling audio capture settings");

    char buf[256];
    if (read_json_body(req, buf, sizeof buf) != ESP_OK) {
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Failed to receive request body");
    }

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Invalid JSON format");
    }

    const cJSON* sample_rate = cJSON_GetObjectItem(root, "samplingRate");
    if (!cJSON_IsNumber(sample_rate)) {
        cJSON_Delete(root);
        return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Invalid samplingRate field");
    }

    int rate = (int)sample_rate->valuedouble;
    switch (rate) {
        case 8000:
        case 11025:
        case 16000:
        case 22050:
        case 32000:
        case 44100:
            break;
        default:
            cJSON_Delete(root);
            return send_json_error(req, TAG, WEBERR_BAD_REQUEST, "Unsupported samplingRate");
    }

    audio_config_t config = audio_config_get();
    int prev_rate = config.sampling_rate;
    config.sampling_rate = rate;

    if (audio_config_set(&config) != ESP_OK) {
        cJSON_Delete(root);
        return send_json_error(req, TAG, WEBERR_INTERNAL_ERR, "Failed to store audio settings");
    }

    if (config.sampling_rate != prev_rate) {
        ESP_LOGI(TAG, "Sampling rate updated: %d -> %d", prev_rate, config.sampling_rate);
    }

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}
