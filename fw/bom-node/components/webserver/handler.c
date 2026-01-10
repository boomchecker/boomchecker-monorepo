
/**
 * \file            handler.c
 * \brief           API handler
 */

/*
 *
 * This file is part of the FELIX-MB project.
 *
 * Author:          Martin Maxa <maxam@btlnet.com>
 * Version:         v0.1
 */

#include "handler.h"
#include "error_handler.h"
#include "esp_log.h"
#include "slre.h"

#define TAG "SERVER_HANDLER"

/*
 * @bruief           Route request to appropriate handler
 * @param[in]       req: Pointer to the HTTP request
 * @param[in]       route_table: Pointer to the route table
 * @param[in]       route_table_size: Size of the route table
 * @return          ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t route_request(httpd_req_t* req, const route_entry_t* route_table,
                        size_t route_table_size) {
    struct slre_cap caps[1]; // Buffer for regex capture groups

    for (size_t i = 0; i < route_table_size; i++) {
        if (slre_match(route_table[i].path, req->uri, strlen(req->uri), caps, 1, 0) > 0) {
            ESP_LOGI(TAG, "Routing to handler: %s", route_table[i].path);
            return route_table[i].handler(req);
        }
    }

    send_json_error(req, TAG, WEBERR_NOT_FOUND, "Endpoint not found");
    return ESP_FAIL;
}