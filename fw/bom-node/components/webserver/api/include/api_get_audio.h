/**
 * \file            api_get_audio.h
 * \brief           API GET AUDIO header file
 */
#ifndef API_GET_AUDIO_HDR_H
#define API_GET_AUDIO_HDR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "esp_http_server.h"

esp_err_t api_get_audio(httpd_req_t* req);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* API_GET_AUDIO_HDR_H */
