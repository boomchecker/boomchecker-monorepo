#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"

#include "handler_options.h"
#include "slre.h"

//////////////////////////////
// Implementations of handlers
//////////////////////////////

// Handler for OPTIONS requests to support CORS
esp_err_t options_handler(httpd_req_t* req) {
    // Set CORS headers
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "http://127.0.0.1:5173");

    // Send an empty response with a 200 status code
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}