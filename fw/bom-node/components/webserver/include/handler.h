/**
 * \file            handler.h
 * \brief           API handler 
 * \details         This file contains the structes for the API handler.
 */

/*
 *
 * This file is part of the FELIX-MB project.
 *
 * Author:          Martin Maxa <martin.maxa@resonect.cz>
 * Version:         v0.1
 */
#ifndef HANDLER_HDR_H
#define HANDLER_HDR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "esp_http_server.h"

// Structure for routing table
typedef struct {
    const char* path;
    esp_err_t (*handler)(httpd_req_t* req);
} route_entry_t;

// Function prototypes, name aligned, lowercase names
esp_err_t route_request(httpd_req_t* req, const route_entry_t* route_table,
                        size_t route_table_size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* HANDLER_HDR_H */