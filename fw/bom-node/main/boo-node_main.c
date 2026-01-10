#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "middleware.h"
#include "webserver.h"

#include "detector.h"
#include "audio_config.h"
#include "mic_input.h"
#include "audio_streamer.h"
#include "ota.h"
#include "ring_buffer.h"

static const char *TAG = "MAIN";

void app_main(void) {
  httpd_handle_t server = NULL;

  esp_err_t err = middleware_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Middleware init failed: %s", esp_err_to_name(err));
  }

  server = start_webserver();
  if (!server) {
    ESP_LOGE(TAG, "Webserver init failed");
  }

#ifdef CONFIG_OTA_ENABLE
  err = ota_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA init failed: %s", esp_err_to_name(err));
  } else {
    ota_check_for_update();
  }
#endif

  audio_config_t audio_cfg = audio_config_get();
  mic_config mic_cfg = {
      .sampling_freq = audio_cfg.sampling_rate > 0 ? audio_cfg.sampling_rate
                                                   : MIC_SAMPLING_FREQUENCY,
      .pre_event_ms = MIC_PRE_EVENT_MS,
      .post_event_ms = MIC_POST_EVENT_MS,
      .num_taps = MIC_DEFAULT_NUM_TAPS,
      .tap_size = MIC_DEFAULT_TAP_SIZE,
  };
  mic_init(&mic_cfg);
  audio_streamer_init();
  mic_start();
  // Impulse detection disabled to keep audio streaming responsive for now.
  // impulse_detector_start();

  while (1) {
    vTaskDelay(1);
  }
}
