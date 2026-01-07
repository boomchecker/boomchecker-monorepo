#include "middleware.h"

#include "wifi.h"

esp_err_t middleware_init(void)
{
    return wifi_init();
}
