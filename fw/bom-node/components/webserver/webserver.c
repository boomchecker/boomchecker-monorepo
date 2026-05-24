#include <stdio.h>
#include "esp_log.h"

#include "endpoints.h"
#include "spiffs_init.h"
#include "webserver.h"

static const char* TAG = "webserver";

/**
 * @brief Stops the ESP32 HTTP web server.
 * 
 * This function shuts down the web server and frees up allocated resources.
 * 
 * @param server Handle to the HTTP server instance to stop.
 */
void stop_webserver(httpd_handle_t server) {
    if (server) {
        ESP_LOGI(TAG, "Stopping webserver");
        httpd_stop(server);
    }
}

/**
 * @brief Starts the ESP32 HTTP web server.
 * 
 * This function initializes and starts the HTTP server, registering all endpoints.
 * 
 * @return Handle to the HTTP server instance, or NULL on failure.
 */
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    config.uri_match_fn = httpd_uri_match_wildcard;

    init_spiffs_static();

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_ERROR_CHECK(register_endpoints(server));
    }

    return server;
}