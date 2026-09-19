/**
 * \file            api_post_audio.h
 * \brief           API POST AUDIO header file
 */
#ifndef API_POST_AUDIO_HDR_H
#define API_POST_AUDIO_HDR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "esp_http_server.h"

esp_err_t api_post_audio(httpd_req_t* req);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* API_POST_AUDIO_HDR_H */
