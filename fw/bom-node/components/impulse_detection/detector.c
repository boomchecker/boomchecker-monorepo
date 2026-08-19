/**
 * @brief Start the impulse detection subsystem.
 *
 * This function configures and starts the impulse detector for both
 * left and right channels. It:
 *  - Reads the microphone configuration via mic_get_config().
 *  - Computes the pre/post event window within the stored tap buffer.
 *  - Initializes the internal impulse_detector instances.
 *  - Registers impulse_detection_on_tap as the microphone tap callback.
 *  - Starts the microphone stream using mic_start().
 *  - Creates and pins the impulse_detection_task FreeRTOS task.
 *
 * Usage requirements:
 *  - mic_init() must be called successfully before calling this function,
 *    so that mic_get_config() returns a valid configuration.
 *
 * Side effects:
 *  - Starts audio capture.
 *  - Spawns a background task that continuously processes tap data and
 *    logs when an impulse is detected.
 */

#include "detector.h"
#include "angle_estimation.h"
#include "median_detection.h"
#include "mic_input.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static const char *TAG = "IMPULSE";

enum { MAX_EVENT_SAMPLES = TAP_COUNT * TAP_SIZE };

static impulse_detector detL;
static impulse_detector detR;
static SemaphoreHandle_t detection_sem = NULL;
static int16_t arrL[MAX_EVENT_SAMPLES];
static int16_t arrR[MAX_EVENT_SAMPLES];
static int wanted_window_start = 0;
static int wanted_window_length = 0;

static void impulse_detection_on_tap(const int16_t *tap_left,
                                     const int16_t *tap_right, void *ctx) {
  (void)ctx;
  if (tap_left == NULL || tap_right == NULL) {
    ESP_LOGE(TAG, "tap callback received NULL buffer");
    return;
  }
  impulse_add_tap(&detL, tap_left);
  impulse_add_tap(&detR, tap_right);
  if (detection_sem != NULL) {
    xSemaphoreGive(detection_sem);
  }
}

static void impulse_detection_task(void *arg) {
  (void)arg;
  bool detectedL = false;
  bool detectedR = false;

  vTaskDelay(pdMS_TO_TICKS(200));
  ESP_LOGI(TAG, "Initialization finished");

  while (1) {
    if (xSemaphoreTake(detection_sem, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    mic_save_event(arrL, arrR);

    detectedL = impulse_run_detection(&detL);
    detectedR = false;
    if (!detectedL) {
      detectedR = impulse_run_detection(&detR);
    }

    if (detectedL || detectedR) {
      ESP_LOGI(TAG, ">>> IMPULSE DETECTED <<<");

      tdoa_estimation tdoa1 =
          cross_corr(arrL, arrR, MAX_EVENT_SAMPLES, MIC_SAMPLING_FREQUENCY);

      float angle1 = calculate_aoa(tdoa1);

      ESP_LOGI(TAG, "CROSS_CORR");
      ESP_LOGI(TAG, "Analysis:");
      ESP_LOGI(TAG,
               " -> Lag: %d samples "
               "(%.3f ms)",
               tdoa1.lag_samples, tdoa1.delay_ms);
      ESP_LOGI(TAG, " -> ANGLE: %.1f degrees", angle1);

      tdoa_estimation tdoa2 = cross_corr_parabolic(
          arrL, arrR, MAX_EVENT_SAMPLES, MIC_SAMPLING_FREQUENCY);

      float angle2 = calculate_aoa(tdoa2);

      ESP_LOGI(TAG, "CROSS_CORR_PARBOLIC");
      ESP_LOGI(TAG, "Analysis:");
      ESP_LOGI(TAG,
               " -> Lag: %d samples "
               "(%.3f ms)",
               tdoa2.lag_samples, tdoa2.delay_ms);
      ESP_LOGI(TAG, " -> ANGLE: %.1f degrees", angle2);

      int16_t *arrL_cutted = &arrL[wanted_window_start];
      int16_t *arrR_cutted = &arrR[wanted_window_start];
      int cutted_len = wanted_window_length;

      tdoa_estimation tdoa3 = pbde_basic(arrL_cutted, arrR_cutted, cutted_len,
                                         MIC_SAMPLING_FREQUENCY);

      float angle3 = calculate_aoa(tdoa3);

      ESP_LOGI(TAG, "PBDE_BASIC");
      ESP_LOGI(TAG, "Analysis:");
      ESP_LOGI(TAG,
               " -> Lag: %d samples "
               "(%.3f ms)",
               tdoa3.lag_samples, tdoa3.delay_ms);
      ESP_LOGI(TAG, " -> ANGLE: %.1f degrees", angle3);

      tdoa_estimation tdoa4 = pbde_lin_reg(arrL_cutted, arrR_cutted, cutted_len,
                                           MIC_SAMPLING_FREQUENCY);

      float angle4 = calculate_aoa(tdoa4);

      ESP_LOGI(TAG, "PBDE_LIN_REG");
      ESP_LOGI(TAG, "Analysis:");
      ESP_LOGI(TAG,
               " -> Lag: %d samples "
               "(%.3f ms)",
               tdoa4.lag_samples, tdoa4.delay_ms);
      ESP_LOGI(TAG, " -> ANGLE: %.1f degrees", angle4);

      if (wanted_window_start < 0 ||
          (wanted_window_start + cutted_len) > MAX_EVENT_SAMPLES) {
        ESP_LOGE(TAG, "Window out of bounds! Using full buffer instead.");
        arrL_cutted = arrL;
        arrR_cutted = arrR;
        cutted_len = MAX_EVENT_SAMPLES;
      }

      // DATA PRINTER
      printf("\n---DATA_START---\n");

      // LEFT CHANNEL
      printf("L:");
      for (int i = 0; i < wanted_window_length; i++) {
        printf("%d,", arrL_cutted[i]);
      }
      printf("\n");

      // RIGHT CHANNEL
      printf("R:");
      for (int i = 0; i < wanted_window_length; i++) {
        printf("%d,", arrR_cutted[i]);
      }
      printf("\n");

      printf("---DATA_END---\n\n");
      // ----------------------------------

      vTaskDelay(pdMS_TO_TICKS(100));

      detectedL = false;
      detectedR = false;

      const int arr_len = MAX_EVENT_SAMPLES;
      if (!((wanted_window_start >= 0) &&
            (wanted_window_start + wanted_window_length <= arr_len))) {
        ESP_LOGE(TAG,
                 "Window out of bounds: "
                 "start=%d, length=%d, "
                 "array size=%d",
                 wanted_window_start, wanted_window_length, arr_len);
      }
    }
  }
}

void impulse_detector_start(void) {
  const mic_config *cfg = mic_get_config();
  if (cfg == NULL) {
    ESP_LOGE(TAG, "mic_get_config failed; call mic_init first");
    return;
  }
  if ((cfg->num_taps != TAP_COUNT) || (cfg->tap_size != TAP_SIZE)) {
    ESP_LOGE(TAG,
             "mic config mismatch: num_taps=%d tap_size=%d "
             "(expected %d/%d)",
             cfg->num_taps, cfg->tap_size, TAP_COUNT, TAP_SIZE);
    return;
  }

  wanted_window_start =
      ((cfg->num_taps * cfg->tap_size) / 2) -
      (int)roundf(cfg->pre_event_ms * (float)cfg->sampling_freq / 1000.0f);
  ESP_LOGI(TAG, "wws - %d", wanted_window_start);

  wanted_window_length = (int)roundf((cfg->pre_event_ms + cfg->post_event_ms) *
                                     (float)cfg->sampling_freq / 1000.0f);
  ESP_LOGI(TAG, "wwl - %d", wanted_window_length);

  impulse_detection_init(&detL);
  impulse_detection_init(&detR);

  if (detection_sem == NULL) {
    detection_sem = xSemaphoreCreateBinary();
    if (detection_sem == NULL) {
      ESP_LOGE(TAG, "Failed to create detection semaphore");
      return;
    }
  }
  mic_add_tap_callback(impulse_detection_on_tap, NULL);
  mic_start();

  BaseType_t task_result = xTaskCreatePinnedToCore(
      impulse_detection_task, "impulse_detection", 8192, NULL, 9, NULL, 0);
  if (task_result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create impulse_detection task (error=%d)",
             (int)task_result);
  }
}
