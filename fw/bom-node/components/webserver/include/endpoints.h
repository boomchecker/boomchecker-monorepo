#ifndef ENDPOINTS_H
#define ENDPOINTS_H

#include "esp_http_server.h"

esp_err_t register_endpoints(httpd_handle_t server);

#endif // ENDPOINTS_H
