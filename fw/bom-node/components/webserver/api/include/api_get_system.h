#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t api_get_system(httpd_req_t* req);
