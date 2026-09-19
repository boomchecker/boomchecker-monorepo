#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "api_get_audio.h"
#include "audio_config.h"
#include "audio_streamer.h"
#include "audio_wav.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "handler.h"
#include "slre.h"

static const char* TAG = "GET_AUDIO";

// Definition of handlers
esp_err_t get_audio_stream_config(httpd_req_t* req);
esp_err_t get_audio_settings(httpd_req_t* req);
esp_err_t get_audio_stream(httpd_req_t* req);
esp_err_t get_audio_stats(httpd_req_t* req);

// Table of routes
static const route_entry_t route_table[] = {
    {"^/api/v1/audio/stream\\.wav$", get_audio_stream},
    {"^/api/v1/audio/stats/?$", get_audio_stats},
    {"^/api/v1/audio/stream/?$", get_audio_stream_config},
    {"^/api/v1/audio/settings/?$", get_audio_settings},
};

// Main handler for GET audio/* requests
esp_err_t api_get_audio(httpd_req_t* req) {
    ESP_LOGI(TAG, "Received GET request: %s", req->uri);

    return route_request(req, route_table, sizeof(route_table) / sizeof(route_entry_t));
}

/**
 * GET /api/v1/audio/stream
 * @summary Get audio stream configuration
 * @tag Audio
 * @response 200 - Audio stream configuration
 * @response 500 - Internal error
 */
esp_err_t get_audio_stream_config(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return httpd_resp_send_500(req);
    }

    audio_config_t config = audio_config_get();
    cJSON_AddStringToObject(root, "mode", config.mode);
    cJSON_AddStringToObject(root, "uploadUrl", config.upload_url);
    cJSON_AddBoolToObject(root, "enabled", config.enabled);

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

/**
 * GET /api/v1/audio/settings
 * @summary Get audio capture settings
 * @tag Audio
 * @response 200 - Audio capture settings
 * @response 500 - Internal error
 */
esp_err_t get_audio_settings(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return httpd_resp_send_500(req);
    }

    audio_config_t config = audio_config_get();
    cJSON_AddNumberToObject(root, "samplingRate", config.sampling_rate);
    cJSON_AddStringToObject(root, "captureMode", "continuous");

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

/**
 * GET /api/v1/audio/stats
 * @summary Get audio streaming statistics
 * @tag Audio
 * @response 200 - Audio statistics
 * @response 500 - Internal error
 */
esp_err_t get_audio_stats(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return httpd_resp_send_500(req);
    }

    audio_streamer_stats_t stats = {0};
    audio_streamer_get_stats(&stats);
    
    cJSON_AddNumberToObject(root, "tapCalls", stats.tap_calls);
    cJSON_AddNumberToObject(root, "streamWrites", stats.stream_writes);
    cJSON_AddNumberToObject(root, "sendFailed", stats.send_failed);
    cJSON_AddNumberToObject(root, "readCalls", stats.read_calls);
    cJSON_AddNumberToObject(root, "readBytes", stats.read_bytes);
    cJSON_AddBoolToObject(root, "pullEnabled", stats.pull_enabled);

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

esp_err_t get_audio_stream(httpd_req_t* req) {
    if (!audio_streamer_pull_enabled()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Audio stream disabled");
        return ESP_FAIL;
    }

    if (!audio_streamer_pull_claim()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Stream already in use");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "audio/wav");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    uint8_t header[44] = {0};
    audio_wav_build_header(header, audio_streamer_sample_rate());
    if (httpd_resp_send_chunk(req, (const char*)header, sizeof(header)) != ESP_OK) {
        goto cleanup;
    }

    uint8_t buf[512];
    while (audio_streamer_pull_enabled()) {
        size_t got = audio_streamer_pull_read(buf, sizeof(buf), pdMS_TO_TICKS(200));
        if (got == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        if (httpd_resp_send_chunk(req, (const char*)buf, got) != ESP_OK) {
            break;
        }
    }
cleanup:
    httpd_resp_send_chunk(req, NULL, 0);
    audio_streamer_pull_release();
    return ESP_OK;
}
