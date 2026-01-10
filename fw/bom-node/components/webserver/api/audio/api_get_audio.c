#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"

#include "api_get_audio.h"
#include "audio_config.h"
#include "cJSON.h"
#include "handler.h"
#include "slre.h"

static const char* TAG = "GET_AUDIO";

// Definition of handlers
esp_err_t get_audio_config(httpd_req_t* req);

// Table of routes
static const route_entry_t route_table[] = {{"^/api/v1/audio/?$", get_audio_config}};

// Main handler for GET audio/* requests
esp_err_t api_get_audio(httpd_req_t* req) {
    ESP_LOGI(TAG, "Received GET request: %s", req->uri);

    return route_request(req, route_table, sizeof(route_table) / sizeof(route_entry_t));
}

/**
 * GET /api/v1/audio
 * @summary Get audio configuration
 * @tag Audio
 * @response 200 - Audio configuration
 * @response 500 - Internal error
 */
esp_err_t get_audio_config(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return httpd_resp_send_500(req);
    }

    audio_config_t config = audio_config_get();
    cJSON_AddStringToObject(root, "mode", config.mode);
    cJSON_AddStringToObject(root, "uploadUrl", config.upload_url);

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
