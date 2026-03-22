#include "angle_estimation.h"
#include "dsp_common.h"
#include "dsps_fft2r.h"
#include "dsps_view.h"
#include "dsps_wind_hann.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_heap_caps.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

tdoa_estimation cross_corr_tdoa(const int16_t *signal1, const int16_t *signal2,
                                int length, int fs) {

  tdoa_estimation res = {0};
  int64_t max_corr = 0;
  int best_lag = 0;
  int max_lag_search =
      (int)ceilf(MIC_DISTANCE_METERS / SPEED_OF_SOUND_MPS * (float)fs);
  printf("MAX_LAG_SEARCH - %d\n", max_lag_search);

  for (int lag = -max_lag_search; lag <= max_lag_search; lag++) {
    int64_t current_corr = 0;
    for (int i = 0; i < length; i++) {
      int j = i + lag;
      if (j >= 0 && j < length) {
        current_corr += (int64_t)signal1[i] * (int64_t)signal2[j];
      }
    }
    if (current_corr > max_corr) {
      max_corr = current_corr;
      best_lag = lag;
    }
  }

  res.lag_samples = best_lag;
  res.delay_ms = ((float)best_lag / (float)fs) * 1000.0f;
  return res;
}

// AoA calculation based on TDOA estimation
float calculate_aoa(tdoa_estimation tdoa) {

  float delay_sec = tdoa.delay_ms / 1000.0f;

  float argument = (SPEED_OF_SOUND_MPS * delay_sec) / MIC_DISTANCE_METERS;

  if (argument > 1.0f) {
    argument = 1.0f;
  } else if (argument < -1.0f) {
    argument = -1.0f;
  }

  float angle_rad = asinf(argument);
  float angle_deg = angle_rad * (180.0f / M_PI);

  return angle_deg;
}

static float *fft_buffer1 = NULL;
static float *fft_buffer2 = NULL;
static float *window = NULL;
static bool is_initialized = false;

void pbde_estimator_init() {
  if (!is_initialized) {
    fft_buffer1 = (float *)heap_caps_malloc(
        TOTAL_SAMPLES * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    fft_buffer2 = (float *)heap_caps_malloc(
        TOTAL_SAMPLES * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    window = (float *)heap_caps_malloc(FFT_SIZE * sizeof(float),
                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (fft_buffer1 == NULL || fft_buffer2 == NULL || window == NULL) {
      ESP_LOGE("ANGLE", "Failed to allocate DSP buffers on heap!");
      return;
    }

    dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    is_initialized = true;
    ESP_LOGI("ANGLE", "DSP buffers allocated and initialized");
  }
}

tdoa_estimation pbde_tdoa(const int16_t *signal1, const int16_t *signal2,
                          int length, int fs) {
  pbde_estimator_init();
  tdoa_estimation res = {0};
  if (!is_initialized)
    return res;

  if (length > FFT_SIZE)
    length = FFT_SIZE;

  generate_root_hann_kaiser_window(window, length);

  // applying window
  for (int i = 0; i < FFT_SIZE; i++) {
    fft_buffer1[i * 2] = (i < length) ? (float)signal1[i] * window[i] : 0.0f;
    fft_buffer2[i * 2] = (i < length) ? (float)signal2[i] * window[i] : 0.0f;
    fft_buffer1[i * 2 + 1] = 0;
    fft_buffer2[i * 2 + 1] = 0;
  }

  // FFT
  dsps_fft2r_fc32(fft_buffer1, FFT_SIZE);
  dsps_fft2r_fc32(fft_buffer2, FFT_SIZE);
  dsps_bit_rev_fc32(fft_buffer1, FFT_SIZE);
  dsps_bit_rev_fc32(fft_buffer2, FFT_SIZE);

  float max_cross_amplitude = 0.0f;
  float norm_weights[FFT_SIZE / 2];
  float t_delay[FFT_SIZE / 2];

  float prev_wrapped_phase = 0.0f;
  float unwrapped_phase = 0.0f;

  for (int i = 1; i < FFT_SIZE / 2; i++) {
    float re1 = fft_buffer1[i * 2], im1 = fft_buffer1[i * 2 + 1];
    float re2 = fft_buffer2[i * 2], im2 = fft_buffer2[i * 2 + 1];

    float phaseX = atan2f(im1, re1);
    float phaseY = atan2f(im2, re2);
    float diff = phaseX - phaseY;

    while (diff > M_PI)
      diff -= 2.0f * M_PI;
    while (diff < -M_PI)
      diff += 2.0f * M_PI;

    if (i == 1) {
      unwrapped_phase = diff;
    } else {
      float phase_step = diff - prev_wrapped_phase;

      while (phase_step > M_PI)
        phase_step -= 2.0f * M_PI;
      while (phase_step < -M_PI)
        phase_step += 2.0f * M_PI;

      unwrapped_phase += phase_step;
    }
    prev_wrapped_phase = diff;

    float f = ((float)i / (float)FFT_SIZE) * (float)fs;
    t_delay[i] = -unwrapped_phase / (2.0f * M_PI * f);

    float amp1 = sqrtf(re1 * re1 + im1 * im1);
    float amp2 = sqrtf(re2 * re2 + im2 * im2);
    float amp = amp1 * amp2;

    norm_weights[i] = amp;

    if (amp > max_cross_amplitude) {
      max_cross_amplitude = amp;
    }
  }

  float final_delay_sec = 0.0f;
  float total_weight = 0.0f;

  // definition of analytical bandwidth
  float f_min = PBDE_F_MIN;
  float f_max = PBDE_F_MAX;

  int bin_min = (int)(f_min * FFT_SIZE / fs);
  int bin_max = (int)(f_max * FFT_SIZE / fs);

  if (bin_min < 1)
    bin_min = 1;
  if (bin_max >= FFT_SIZE / 2)
    bin_max = FFT_SIZE / 2 - 1;

  for (int i = bin_min; i <= bin_max; i++) {
    // TODO: q - scaling function - now equals 1 for whole spectrum
    float normalized_weight = norm_weights[i] / max_cross_amplitude;

    final_delay_sec += t_delay[i] * normalized_weight;
    total_weight += normalized_weight;
  }

  final_delay_sec = final_delay_sec / total_weight;

  res.delay_ms = final_delay_sec * 1000.0f;
  res.lag_samples = (int)roundf(final_delay_sec * fs);

  return res;
}

static float bessel_i0f(float x) {
  float sum = 1.0f, term = 1.0f;
  float x2_over_4 = (x * x) / 4.0f;
  for (int i = 1; i < 20; i++) {
    term *= x2_over_4 / (float)(i * i);
    sum += term;
    if (term < sum * 1e-7f)
      break;
  }
  return sum;
}

void generate_root_hann_kaiser_window(float *w, int L) {
  // parameters of windows: L = total length, M = 0.8 * L, beta = 12
  int M = (int)(0.8f * L);
  float beta = 12.0f;
  int transition_point = L - M;

  // Root-Hanning
  int hanning_len = 2 * (L - M);
  for (int n = 0; n < transition_point; n++) {
    float hann =
        0.5f *
        (1.0f - cosf(2.0f * (float)M_PI * (float)n / (float)hanning_len));
    w[n] = sqrtf(hann);
  }

  // Root-Kaiser
  double i0_beta = bessel_i0f(beta);
  int kaiser_len = 2 * M;
  for (int n = transition_point; n < L; n++) {
    int k_idx = n - (L - M) + M;
    float arg = 2.0f * (float)k_idx / (float)kaiser_len - 1.0f;
    float x = beta * sqrtf(1.0f - arg * arg);
    float kaiser = bessel_i0f(x) / i0_beta;
    w[n] = sqrtf(kaiser);
  }
}
