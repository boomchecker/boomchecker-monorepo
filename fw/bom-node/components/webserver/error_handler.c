/**
 * \file            error_handler.c
 * \brief           Error handler for webserver
 * \details         This file contains the implementation of the error handler for the webserver.
 */

/*
 * This file is part of Felix-MB project.
 *
 * Author:          Martin Maxa <martin.maxa@resonect.cz>
 */
#include <string.h>
#include "cJSON.h"
#include "error_handler.h"

struct webserver_error webserver_error_create(const char* tag, enum webserver_errors code,
                                              const char* message) {
    struct webserver_error err = {.tag = tag, .code = code, .message = message};
    return err;
}

esp_err_t webserver_error_to_json(const struct webserver_error* err, char* buf, size_t buf_size) {
    if (!err || !buf || buf_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(root, "tag", err->tag);
    cJSON_AddNumberToObject(root, "code", (int)err->code);
    cJSON_AddStringToObject(root, "message", err->message);

    char* json_str = cJSON_PrintUnformatted(root);
    esp_err_t result = ESP_FAIL;

    if (json_str) {
        size_t len = strlen(json_str);
        if (len < buf_size) {
            memcpy(buf, json_str, len + 1);
            result = ESP_OK;
        } else {
            result = ESP_ERR_NO_MEM;
        }
        cJSON_free(json_str);
    }

    cJSON_Delete(root);
    return result;
}

esp_err_t send_json_error(httpd_req_t* req, const char* tag, enum webserver_errors code,
                          const char* message) {
    char json_buf[128];

    struct webserver_error err = webserver_error_create(tag, code, message);

    if (webserver_error_to_json(&err, json_buf, sizeof(json_buf)) != ESP_OK) {
        // Fallback if JSON build fails
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal error");
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json_buf, strlen(json_buf));
}