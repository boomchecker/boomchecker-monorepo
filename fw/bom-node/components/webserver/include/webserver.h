#ifndef ESP32_WEBSERVER_H
#define ESP32_WEBSERVER_H

#include "esp_http_server.h"

/**
 * @brief Starts the ESP32 HTTP web server.
 * 
 * This function initializes and starts the web server with predefined
 * endpoints for device management, Wi-Fi setup, module management,
 * alerts, OTA updates, and system management.
 * 
 * @return httpd_handle_t Handle to the HTTP server instance.
 */
httpd_handle_t start_webserver(void);

/**
 * @brief Stops the ESP32 HTTP web server.
 * 
 * This function shuts down the web server and frees up allocated resources.
 * 
 * @param server Handle to the HTTP server instance to stop.
 */
void stop_webserver(httpd_handle_t server);

#endif // ESP32_WEBSERVER_H
