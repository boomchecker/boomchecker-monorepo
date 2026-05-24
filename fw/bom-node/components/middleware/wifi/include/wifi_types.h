#pragma once

#include <stdint.h>
#include "esp_wifi_types.h"

#define MAX_WIFI_SCAN_RESULTS 20

typedef struct
{
    char ssid[32];
    char password[64];
} wifi_credentials_t;

typedef struct
{
    uint16_t count;
    wifi_ap_record_t records[MAX_WIFI_SCAN_RESULTS];
} wifi_scan_result_t;
