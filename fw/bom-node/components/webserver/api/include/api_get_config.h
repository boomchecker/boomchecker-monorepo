/**
 * \file            api_get_device.h
 * \brief           API GET DEVICE header file
 * \details         This file contains the function prototype for the API GET DEVICE handler.
 */

/*
 *
 * This file is part of the FELIX-MB project.
 *
 * Author:          Martin Maxa <martin.maxa@resonect.cz>
 * Version:         v0.1
 */
#ifndef API_GET_CONFIG_HDR_H
#define API_GET_CONFIG_HDR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "esp_http_server.h"

/* Function prototypes, name aligned, lowercase names */
esp_err_t api_get_config(httpd_req_t* req);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* API_GET_CONFIG_HDR_H */