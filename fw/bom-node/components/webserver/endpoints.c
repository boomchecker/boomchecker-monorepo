#include "endpoints.h"

#include "handler.h"
#include "handler_options.h"

#include "api_get_audio.h"
#include "api_get_config.h"
#include "api_get_wifi.h"
#include "api_post_audio.h"
#include "api_post_wifi.h"
#include "handler_get_static.h"

// API Handlers GET
const route_entry_t route_table_api_get[] = {{"^/api/v1/wifi(/.*)?/?$", api_get_wifi},
                                             {"^/api/v1/audio(/.*)?/?$", api_get_audio},
                                             {"^/api/v1/config(/.*)?/?$", api_get_config}};

esp_err_t api_get_handler(httpd_req_t* req) {
    return route_request(req, route_table_api_get,
                         sizeof(route_table_api_get) / sizeof(route_entry_t));
}

// API Handlers POST
const route_entry_t route_table_api_post[] = {{"^/api/v1/wifi(/.*)?/?$", api_post_wifi},
                                              {"^/api/v1/audio(/.*)?/?$", api_post_audio}};

esp_err_t api_post_handler(httpd_req_t* req) {
    return route_request(req, route_table_api_post,
                         sizeof(route_table_api_post) / sizeof(route_entry_t));
}

// URI Handlers
static httpd_uri_t api_get_handler_uri = {
    .uri = "/api/*", .method = HTTP_GET, .handler = api_get_handler, .user_ctx = NULL};

static httpd_uri_t api_post_handler_uri = {
    .uri = "/api/*", .method = HTTP_POST, .handler = api_post_handler, .user_ctx = NULL};

httpd_uri_t options_uri = {
    .uri = "/*", .method = HTTP_OPTIONS, .handler = options_handler, .user_ctx = NULL};

// Static file handler
static httpd_uri_t uri_get_static_tmp = {
    .uri = "/*", .method = HTTP_GET, .handler = get_static_handler, .user_ctx = NULL};

static httpd_uri_t uri_get_static = {
    .uri = "static/*", .method = HTTP_GET, .handler = get_static_handler, .user_ctx = NULL};

// register all URI handlers function
esp_err_t register_endpoints(httpd_handle_t server) {
    // Register API handlers
    httpd_register_uri_handler(server, &api_get_handler_uri);
    httpd_register_uri_handler(server, &api_post_handler_uri);

    // Register OPTIONS handler
    httpd_register_uri_handler(server, &options_uri);

    // Register Static file handler
    httpd_register_uri_handler(server, &uri_get_static_tmp);
    httpd_register_uri_handler(server, &uri_get_static);

    return ESP_OK;
}
