/**
 * \file            api_post_wifi.h
 * \brief           API POST Wi-Fi header file
 * \details         This file contains the function prototype for the API POST Wi-Fi handler.
 */

/*
 *
 * This file is part of the FELIX-MB project.
 *
 * Author:          Martin Maxa <martin.maxa@resonect.cz>
 * Version:         v0.1
 */
#ifndef API_POST_WIFI_HDR_H
#define API_POST_WIFI_HDR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "esp_http_server.h"

/* Function prototypes, name aligned, lowercase names */
esp_err_t api_post_wifi(httpd_req_t* req);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* API_POST_WIFI_HDR_H */