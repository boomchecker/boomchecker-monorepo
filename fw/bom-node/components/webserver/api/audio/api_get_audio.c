#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "api_get_audio.h"
#include "audio_config.h"
#include "audio_streamer.h"
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

static void write_le16(uint8_t* dst, uint16_t val) {
    dst[0] = (uint8_t)(val & 0xff);
    dst[1] = (uint8_t)((val >> 8) & 0xff);
}

static void write_le32(uint8_t* dst, uint32_t val) {
    dst[0] = (uint8_t)(val & 0xff);
    dst[1] = (uint8_t)((val >> 8) & 0xff);
    dst[2] = (uint8_t)((val >> 16) & 0xff);
    dst[3] = (uint8_t)((val >> 24) & 0xff);
}

static void build_wav_header(uint8_t* out, int sample_rate) {
    const uint16_t num_channels = 2;
    const uint16_t bits_per_sample = 16;
    const uint32_t byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    const uint16_t block_align = num_channels * bits_per_sample / 8;
    const uint32_t data_size = 0xffffffff;
    const uint32_t riff_size = data_size + 36;

    memcpy(out, "RIFF", 4);
    write_le32(out + 4, riff_size);
    memcpy(out + 8, "WAVE", 4);
    memcpy(out + 12, "fmt ", 4);
    write_le32(out + 16, 16);
    write_le16(out + 20, 1);
    write_le16(out + 22, num_channels);
    write_le32(out + 24, (uint32_t)sample_rate);
    write_le32(out + 28, byte_rate);
    write_le16(out + 32, block_align);
    write_le16(out + 34, bits_per_sample);
    memcpy(out + 36, "data", 4);
    write_le32(out + 40, data_size);
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
    build_wav_header(header, audio_streamer_sample_rate());
    if (httpd_resp_send_chunk(req, (const char*)header, sizeof(header)) != ESP_OK) {
        audio_streamer_pull_release();
        return ESP_FAIL;
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
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    httpd_resp_send_chunk(req, NULL, 0);
    audio_streamer_pull_release();
    return ESP_OK;
}
