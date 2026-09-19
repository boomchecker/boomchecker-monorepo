#include "api_get_system.h"

esp_err_t api_get_system(httpd_req_t* req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}
