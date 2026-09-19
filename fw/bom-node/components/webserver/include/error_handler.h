/**
 * \file            error_handler.h
 * \brief           Error handler header file
 */

/*
 * This file is part of Felix-MB project.
 *
 * Author:          Martin Maxa <martin.maxa@resonect.cz>
 */
#ifndef ERROR_HANDLER_HDR_H
#define ERROR_HANDLER_HDR_H

#include <stddef.h> // Added to define size_t
#include <stdint.h>
#include "esp_err.h"
#include "esp_http_server.h" // Added to define httpd_req_t

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum webserver_errors {
    WEBERR_BAD_REQUEST = 100,
    WEBERR_INTERNAL_ERR,
    WEBERR_NOT_FOUND,
    WEBERR_API_NOT_FOUND = 300
};

/*
 * \brief           Error handler for webserver
 * \details         This file contains the implementation of the error handler for the webserver.
 */
struct webserver_error {
    const char* tag;            // New: subsystem or module, e.g. "wifi", "sensor", "auth"
    enum webserver_errors code; // Error code
    const char* message;        // Human-readable message
};

/* Function prototypes, name aligned, lowercase names */
/*
 * \brief           Create an API error
 * \param[in]       code: Error code
 * \param[in]       message: Error message
 * \return          api_error_t: Created API error
 */
struct webserver_error webserver_error_create(const char* tag, enum webserver_errors code,
                                              const char* message);

/*
 * \brief           Convert API error to JSON string
 * \param[in]       err: API error to convert
 * \param[out]      buf: Buffer to hold the JSON string
 * \param[in]       buf_size: Size of the buffer
 */
esp_err_t webserver_error_to_json(const struct webserver_error* err, char* buf, size_t buf_size);

/**
 * @brief Sends a JSON error response.
 * 
 * @param req The HTTP request object.
 * @param http_status The HTTP status code to send.
 * @param tag The subsystem or module name.
 * @param code The error code within the tag.
 * @param message The human-readable error message.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t send_json_error(httpd_req_t* req, const char* tag, enum webserver_errors code,
                          const char* message);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ERROR_HANDLER_HDR_H */