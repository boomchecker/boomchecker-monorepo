#ifndef API_GET_WIFI_H
#define API_GET_WIFI_H

#include "esp_http_server.h"

esp_err_t api_get_wifi(httpd_req_t* req);

#endif // API_GET_WIFI_H