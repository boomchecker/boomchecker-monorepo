#ifndef HANDLER_OPTIONS_H
#define HANDLER_OPTIONS_H

#include "esp_http_server.h"

// Handler for OPTIONS requests to support CORS
esp_err_t options_handler(httpd_req_t* req);

#endif // HANDLER_OPTIONS_H
