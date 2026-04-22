#include "angle_estimation.h"
#include "esp_dsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_heap_caps.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NUM_SEGMENTS
#define NUM_SEGMENTS 100
#endif
#ifndef OVERLAP_RATIO
#define OVERLAP_RATIO 0.5f
#endif
#ifndef CHECK_WINDOW_N
#define CHECK_WINDOW_N 6
#endif
#ifndef DEVIATION_THRESHOLD
#define DEVIATION_THRESHOLD 0.6f
#endif
#ifndef SMOOTH_CORRECTION_WINDOW
#define SMOOTH_CORRECTION_WINDOW 3
#endif

static float *fft_data1 = NULL;
static float *fft_data2 = NULL;
static float *window_arr = NULL;
static bool is_initialized = false;

float calculate_aoa(tdoa_estimation tdoa) {
  float delay_sec = tdoa.delay_ms / 1000.0f;
  float argument = (SPEED_OF_SOUND_MPS * delay_sec) / MIC_DISTANCE_METERS;
  if (argument > 1.0f)
    argument = 1.0f;
  else if (argument < -1.0f)
    argument = -1.0f;
  return asinf(argument) * (180.0f / M_PI);
}

void fft_init() {
  if (!is_initialized) {
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    if (ret != ESP_OK) {
      ESP_LOGE("PBDE", "Failed to initialize ESP-DSP FFT: %s",
               esp_err_to_name(ret));
      return;
    }

    fft_data1 = malloc(FFT_SIZE * 2 * sizeof(float));
    fft_data2 = malloc(FFT_SIZE * 2 * sizeof(float));
    window_arr = (float *)heap_caps_malloc(
        FFT_SIZE * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    ESP_LOGI("FFT", "Free heap: %u bytes",
             (unsigned int)esp_get_free_heap_size());

    if (!fft_data1 || !fft_data2 || !window_arr) {
      ESP_LOGE("FFT", "Insufficient memory for ESP-DSP buffers!");
      return;
    }

    is_initialized = true;
    ESP_LOGI("FFT", "ESP-DSP buffers allocated and initialized.");
  }
}

// basic cross correlation
tdoa_estimation cross_corr(const int16_t *signal1, const int16_t *signal2,
                           int length, int fs) {
  fft_init();
  tdoa_estimation res = {0};

  int nfft = FFT_SIZE;

  for (int i = 0; i < nfft; i++) {
    if (i < length) {
      fft_data1[i * 2] = (float)signal1[i];
      fft_data1[i * 2 + 1] = 0.0f;
      fft_data2[i * 2] = (float)signal2[i];
      fft_data2[i * 2 + 1] = 0.0f;
    } else {
      fft_data1[i * 2] = 0.0f;
      fft_data1[i * 2 + 1] = 0.0f;
      fft_data2[i * 2] = 0.0f;
      fft_data2[i * 2 + 1] = 0.0f;
    }
  }

  dsps_fft2r_fc32(fft_data1, nfft);
  dsps_bit_rev_fc32(fft_data1, nfft);

  dsps_fft2r_fc32(fft_data2, nfft);
  dsps_bit_rev_fc32(fft_data2, nfft);

  for (int i = 0; i < nfft; i++) {
    float re1 = fft_data1[i * 2];
    float im1 = fft_data1[i * 2 + 1];
    float re2 = fft_data2[i * 2];
    float im2 = fft_data2[i * 2 + 1];

    float r_re = re1 * re2 + im1 * im2;
    float r_im = im1 * re2 - re1 * im2;

    fft_data1[i * 2] = r_re;
    fft_data1[i * 2 + 1] = -r_im;
  }

  dsps_fft2r_fc32(fft_data1, nfft);
  dsps_bit_rev_fc32(fft_data1, nfft);

  int max_lag_search =
      (int)ceilf(MIC_DISTANCE_METERS / SPEED_OF_SOUND_MPS * (float)fs) + 3;
  float max_corr = -1e20f;
  int best_lag = 0;

  for (int lag = -max_lag_search; lag <= max_lag_search; lag++) {
    int idx = (lag < 0) ? (nfft + lag) : lag;
    float current_corr = fft_data1[idx * 2];

    if (current_corr > max_corr) {
      max_corr = current_corr;
      best_lag = lag;
    }
  }

  res.lag_samples = best_lag;
  res.delay_ms = ((float)best_lag / (float)fs) * 1000.0f;

  // DATA PRINTER
  printf("\n---CROSS_CORR_START---\n");
  printf("LAGS:");
  for (int lag = -max_lag_search; lag <= max_lag_search; lag++) {
    printf("%d,", lag);
  }
  printf("\nCORR:");
  for (int lag = -max_lag_search; lag <= max_lag_search; lag++) {
    int idx = (lag < 0) ? (nfft + lag) : lag;
    printf("%f,", fft_data1[idx * 2]);
  }
  printf("\n---CROSS_CORR_END---\n");

  return res;
}

// cross correlation with parabola interpolation
tdoa_estimation cross_corr_parabolic(const int16_t *signal1,
                                     const int16_t *signal2, int length,
                                     int fs) {
  fft_init();
  tdoa_estimation res = {0};
  int nfft = FFT_SIZE;

  for (int i = 0; i < nfft; i++) {
    if (i < length) {
      fft_data1[i * 2] = (float)signal1[i];
      fft_data1[i * 2 + 1] = 0.0f;
      fft_data2[i * 2] = (float)signal2[i];
      fft_data2[i * 2 + 1] = 0.0f;
    } else {
      fft_data1[i * 2] = 0.0f;
      fft_data1[i * 2 + 1] = 0.0f;
      fft_data2[i * 2] = 0.0f;
      fft_data2[i * 2 + 1] = 0.0f;
    }
  }

  dsps_fft2r_fc32(fft_data1, nfft);
  dsps_bit_rev_fc32(fft_data1, nfft);
  dsps_fft2r_fc32(fft_data2, nfft);
  dsps_bit_rev_fc32(fft_data2, nfft);

  for (int i = 0; i < nfft; i++) {
    float re1 = fft_data1[i * 2], im1 = fft_data1[i * 2 + 1];
    float re2 = fft_data2[i * 2], im2 = fft_data2[i * 2 + 1];
    float r_re = re1 * re2 + im1 * im2;
    float r_im = im1 * re2 - re1 * im2;
    fft_data1[i * 2] = r_re;
    fft_data1[i * 2 + 1] = -r_im;
  }

  dsps_fft2r_fc32(fft_data1, nfft);
  dsps_bit_rev_fc32(fft_data1, nfft);

  int max_lag_search =
      (int)ceilf(MIC_DISTANCE_METERS / SPEED_OF_SOUND_MPS * (float)fs) + 3;
  float max_corr = -1e20f;
  int best_lag = 0;

  for (int lag = -max_lag_search; lag <= max_lag_search; lag++) {
    int idx = (lag < 0) ? (nfft + lag) : lag;
    float current_corr = fft_data1[idx * 2];
    if (current_corr > max_corr) {
      max_corr = current_corr;
      best_lag = lag;
    }
  }

  int idx_0 = (best_lag < 0) ? (nfft + best_lag) : best_lag;
  int idx_m1 = (best_lag - 1 < 0) ? (nfft + best_lag - 1) : (best_lag - 1);
  int idx_p1 = (best_lag + 1 < 0) ? (nfft + best_lag + 1) : (best_lag + 1);

  float y_minus1 = fft_data1[idx_m1 * 2];
  float y_0 = fft_data1[idx_0 * 2];
  float y_plus1 = fft_data1[idx_p1 * 2];

  float delta = 0.0f;
  float denominator = 2.0f * (y_minus1 - 2.0f * y_0 + y_plus1);
  if (denominator != 0.0f) {
    delta = (y_minus1 - y_plus1) / denominator;
  }

  float final_lag = (float)best_lag + delta;

  res.lag_samples = (int)roundf(final_lag);
  res.delay_ms = (final_lag / (float)fs) * 1000.0f;

  printf("\n---CROSS_CORR_PARABOLIC_START---\n");
  printf("LAGS:");
  for (int lag = -max_lag_search; lag <= max_lag_search; lag++) {
    printf("%d,", lag);
  }
  printf("\nCORR:");
  for (int lag = -max_lag_search; lag <= max_lag_search; lag++) {
    int idx = (lag < 0) ? (nfft + lag) : lag;
    printf("%f,", fft_data1[idx * 2]);
  }
  printf("\nPARABOLA_PTS:%f,%f,%f\n", y_minus1, y_0, y_plus1);
  printf("FINAL_LAG:%f\n", final_lag);
  printf("---CROSS_CORR_PARABOLIC_END---\n");

  return res;
}

// function used in searching for median
static int cmp_float(const void *a, const void *b) {
  float fa = *(const float *)a;
  float fb = *(const float *)b;
  return (fa > fb) - (fa < fb);
}

tdoa_estimation pbde_basic(const int16_t *signal1, const int16_t *signal2,
                           int length, int fs) {
  fft_init();
  tdoa_estimation res = {0};
  if (!is_initialized)
    return res;

  if (length > FFT_SIZE)
    length = FFT_SIZE;

  generate_root_hann_kaiser_window(window_arr, length);

  for (int i = 0; i < FFT_SIZE; i++) {
    if (i < length) {
      fft_data1[i * 2] = (float)signal1[i] * window_arr[i];
      fft_data1[i * 2 + 1] = 0.0f;
      fft_data2[i * 2] = (float)signal2[i] * window_arr[i];
      fft_data2[i * 2 + 1] = 0.0f;
    } else {
      fft_data1[i * 2] = 0.0f;
      fft_data1[i * 2 + 1] = 0.0f;
      fft_data2[i * 2] = 0.0f;
      fft_data2[i * 2 + 1] = 0.0f;
    }
  }

  dsps_fft2r_fc32(fft_data1, FFT_SIZE);
  dsps_bit_rev_fc32(fft_data1, FFT_SIZE);
  dsps_fft2r_fc32(fft_data2, FFT_SIZE);
  dsps_bit_rev_fc32(fft_data2, FFT_SIZE);

  int length_N = FFT_SIZE / 2;
  float max_cross_amplitude = 0.0f;

  float *norm_weights = malloc(length_N * sizeof(float));
  float *u_phase = malloc(length_N * sizeof(float));
  float *t_delay = malloc(length_N * sizeof(float));

  if (!norm_weights || !u_phase || !t_delay) {
    ESP_LOGE("PBDE", "Insufficient RAM for basic PBDE arrays!");
    if (norm_weights)
      free(norm_weights);
    if (u_phase)
      free(u_phase);
    if (t_delay)
      free(t_delay);
    return res;
  }

  float unwrapped_phase = 0.0f;
  float prev_re = 0.0f;
  float prev_im = 0.0f;

  t_delay[0] = 0.0f;

  for (int i = 0; i < length_N; i++) {
    float re1 = fft_data1[i * 2];
    float im1 = fft_data1[i * 2 + 1];
    float re2 = fft_data2[i * 2];
    float im2 = fft_data2[i * 2 + 1];

    float curr_re = re1 * re2 + im1 * im2;
    float curr_im = im1 * re2 - re1 * im2;

    if (i == 0) {
      unwrapped_phase = atan2f(curr_im, curr_re);
    } else {
      float diff_re = curr_re * prev_re + curr_im * prev_im;
      float diff_im = curr_im * prev_re - curr_re * prev_im;
      unwrapped_phase += atan2f(diff_im, diff_re);

      float f = ((float)i / (float)FFT_SIZE) * (float)fs;
      t_delay[i] = -unwrapped_phase / (2.0f * (float)M_PI * f);
    }

    prev_re = curr_re;
    prev_im = curr_im;

    u_phase[i] = unwrapped_phase;

    float amp = sqrtf(curr_re * curr_re + curr_im * curr_im);
    norm_weights[i] = amp;
    if (amp > max_cross_amplitude) {
      max_cross_amplitude = amp;
    }
  }

  int s_bin = (int)roundf((PBDE_F_MIN * (float)FFT_SIZE) / (float)fs);
  int e_bin = (int)roundf((PBDE_F_MAX * (float)FFT_SIZE) / (float)fs);

  if (s_bin < 1)
    s_bin = 1;
  if (e_bin >= length_N)
    e_bin = length_N - 1;
  if (s_bin > e_bin) {
    s_bin = 1;
    e_bin = length_N - 1;
  }

  float final_delay_sec = 0.0f;
  float total_weight = 0.0f;

  for (int i = s_bin; i <= e_bin; i++) {
    float normalized_weight = norm_weights[i] / max_cross_amplitude;
    final_delay_sec += t_delay[i] * normalized_weight;
    total_weight += normalized_weight;
  }

  if (total_weight > 0.0f) {
    final_delay_sec = final_delay_sec / total_weight;
  }

  res.delay_ms = final_delay_sec * 1000.0f;
  res.lag_samples = (int)roundf(final_delay_sec * (float)fs);

  // DATA PRINTER
  printf("\n---PBDE_BASIC_START---\n");
  printf("BINS_USED:%d,%d\n", s_bin, e_bin);
  printf("DELAY:");
  for (int i = 0; i < length_N; i++) {
    printf("%f,", t_delay[i] * 1000.0f);
  }
  printf("\nWEIGHTS:");
  for (int i = 0; i < length_N; i++) {
    printf("%f,", norm_weights[i] / max_cross_amplitude);
  }
  printf("\nPHASE:");
  for (int i = 0; i < length_N; i++) {
    printf("%f,", u_phase[i]);
  }
  printf("\n---PBDE_BASIC_END---\n");

  free(norm_weights);
  free(u_phase);
  free(t_delay);

  return res;
}

tdoa_estimation pbde_lin_reg(const int16_t *signal1, const int16_t *signal2,
                             int length, int fs) {
  fft_init();
  tdoa_estimation res = {0};
  if (!is_initialized)
    return res;

  if (length > FFT_SIZE)
    length = FFT_SIZE;

  generate_root_hann_kaiser_window(window_arr, length);

  for (int i = 0; i < FFT_SIZE; i++) {
    if (i < length) {
      fft_data1[i * 2] = (float)signal1[i] * window_arr[i];
      fft_data1[i * 2 + 1] = 0.0f;

      fft_data2[i * 2] = (float)signal2[i] * window_arr[i];
      fft_data2[i * 2 + 1] = 0.0f;
    } else {
      fft_data1[i * 2] = 0.0f;
      fft_data1[i * 2 + 1] = 0.0f;

      fft_data2[i * 2] = 0.0f;
      fft_data2[i * 2 + 1] = 0.0f;
    }
  }

  dsps_fft2r_fc32(fft_data1, FFT_SIZE);
  dsps_bit_rev_fc32(fft_data1, FFT_SIZE);

  dsps_fft2r_fc32(fft_data2, FFT_SIZE);
  dsps_bit_rev_fc32(fft_data2, FFT_SIZE);

  int length_N = FFT_SIZE / 2;
  float max_cross_amplitude = 0.0f;

  float *norm_weights = malloc(length_N * sizeof(float));
  float *u_phase = malloc(length_N * sizeof(float));
  float *p_step = malloc(length_N * sizeof(float));

  if (!norm_weights || !u_phase || !p_step) {
    ESP_LOGE("PBDE", "Insufficient RAM for phase arrays!");
    if (norm_weights)
      free(norm_weights);
    if (u_phase)
      free(u_phase);
    if (p_step)
      free(p_step);
    return res;
  }

  float unwrapped_phase = 0.0f;
  float prev_re = 0.0f;
  float prev_im = 0.0f;

  for (int i = 0; i < length_N; i++) {
    float re1 = fft_data1[i * 2];
    float im1 = fft_data1[i * 2 + 1];
    float re2 = fft_data2[i * 2];
    float im2 = fft_data2[i * 2 + 1];

    float curr_re = re1 * re2 + im1 * im2;
    float curr_im = im1 * re2 - re1 * im2;

    float phase_step = 0.0f;

    if (i == 0) {
      unwrapped_phase = atan2f(curr_im, curr_re);
      p_step[i] = unwrapped_phase;
    } else {
      float diff_re = curr_re * prev_re + curr_im * prev_im;
      float diff_im = curr_im * prev_re - curr_re * prev_im;
      phase_step = atan2f(diff_im, diff_re);
      unwrapped_phase += phase_step;
      p_step[i] = phase_step;
    }

    prev_re = curr_re;
    prev_im = curr_im;
    u_phase[i] = unwrapped_phase;

    float amp = sqrtf(curr_re * curr_re + curr_im * curr_im);
    norm_weights[i] = amp;
    if (amp > max_cross_amplitude) {
      max_cross_amplitude = amp;
    }
  }

  // initial slope estimation
  float step_ratio = 1.0f - OVERLAP_RATIO;
  int window_size = (int)(length_N / (1.0f + (NUM_SEGMENTS - 1) * step_ratio));
  if (window_size < 2)
    window_size = 2;
  int step_size = (int)(window_size * step_ratio);
  if (step_size < 1)
    step_size = 1;

  float *segment_means = malloc(NUM_SEGMENTS * sizeof(float));
  int num_segment_means = 0;

  for (int i = 0; i < NUM_SEGMENTS; i++) {
    int start_idx = i * step_size;
    if (start_idx == 0)
      start_idx = 1;
    int end_idx = start_idx + window_size;
    if (end_idx > length_N)
      end_idx = length_N;
    if (start_idx >= length_N)
      break;

    float sum = 0;
    int count = 0;
    for (int j = start_idx; j < end_idx; j++) {
      sum += p_step[j];
      count++;
    }
    if (count > 0) {
      segment_means[num_segment_means++] = sum / count;
    }
  }

  float initial_slope = 0.0f;
  if (num_segment_means > 0) {
    qsort(segment_means, num_segment_means, sizeof(float), cmp_float);
    if (num_segment_means % 2 == 0) {
      initial_slope = (segment_means[num_segment_means / 2 - 1] +
                       segment_means[num_segment_means / 2]) /
                      2.0f;
    } else {
      initial_slope = segment_means[num_segment_means / 2];
    }
  }
  free(segment_means);

  ESP_LOGI("PBDE_OPT", "Method 1 (Initial slope - median): %.5f",
           initial_slope);

  // dynamic segmentation
  typedef struct {
    int start;
    int end;
  } block_t;
  block_t *segments = malloc(length_N * sizeof(block_t));
  int num_segments = 0;
  int curr_start = 1;

  for (int i = CHECK_WINDOW_N + 1; i < length_N; i++) {
    float window_sum = 0;
    for (int j = i - CHECK_WINDOW_N; j < i; j++) {
      window_sum += p_step[j];
    }
    float window_mean = window_sum / CHECK_WINDOW_N;

    if (fabsf(window_mean - initial_slope) > DEVIATION_THRESHOLD) {
      int cut_idx = i - CHECK_WINDOW_N;
      if (cut_idx >= curr_start) {
        segments[num_segments++] = (block_t){curr_start, cut_idx};
      }
      curr_start = cut_idx + 1;
    }
  }
  if (curr_start < length_N) {
    segments[num_segments++] = (block_t){curr_start, length_N - 1};
  }

  block_t *valid_blocks = malloc(num_segments * sizeof(block_t));
  int num_valid_blocks = 0;
  for (int i = 0; i < num_segments; i++) {
    if (segments[i].end - segments[i].start >= 1) {
      valid_blocks[num_valid_blocks++] = segments[i];
    }
  }
  free(segments);

  // continuous slope estimation
  int half_length = length_N / 2;
  block_t *fit_blocks_lower = malloc(num_valid_blocks * sizeof(block_t));
  int num_fit_blocks = 0;

  for (int i = 0; i < num_valid_blocks; i++) {
    int s = valid_blocks[i].start;
    int e = valid_blocks[i].end;
    if (s < half_length) {
      fit_blocks_lower[num_fit_blocks].start = s;
      fit_blocks_lower[num_fit_blocks].end =
          (e < half_length - 1) ? e : (half_length - 1);
      if (fit_blocks_lower[num_fit_blocks].end -
              fit_blocks_lower[num_fit_blocks].start >=
          1) {
        num_fit_blocks++;
      }
    }
  }

  float final_slope = initial_slope;
  if (num_fit_blocks == 0) {
    ESP_LOGW("PBDE", "No valid blocks in the lower half. Using initial slope.");
  } else {
    double numerator = 0.0;
    double denominator = 0.0;

    for (int i = 0; i < num_fit_blocks; i++) {
      int s = fit_blocks_lower[i].start;
      int e = fit_blocks_lower[i].end;

      double sum_x = 0, sum_y = 0, sum_w = 0;
      for (int j = s; j <= e; j++) {
        sum_w += norm_weights[j];
        sum_x += j * norm_weights[j];
        sum_y += u_phase[j] * norm_weights[j];
      }

      double mean_x = (sum_w > 0) ? (sum_x / sum_w) : ((s + e) / 2.0);
      double mean_y = (sum_w > 0) ? (sum_y / sum_w) : 0.0;

      for (int j = s; j <= e; j++) {
        double x_c = j - mean_x;
        double y_c = u_phase[j] - mean_y;
        numerator += norm_weights[j] * x_c * y_c;
        denominator += norm_weights[j] * x_c * x_c;
      }
    }
    if (denominator > 0.0)
      final_slope = (float)(numerator / denominator);
  }
  free(fit_blocks_lower);
  free(valid_blocks);

  // phase step correction
  float *corrected_phase_step = malloc(length_N * sizeof(float));
  memcpy(corrected_phase_step, p_step, length_N * sizeof(float));

  float current_abs_phase = corrected_phase_step[0];
  int corrections_made = 0;

  for (int i = 1; i < length_N; i++) {
    float expected_abs_phase = i * final_slope;
    current_abs_phase += corrected_phase_step[i];
    float diff = current_abs_phase - expected_abs_phase;

    if (fabsf(diff) > 0.7f * (float)M_PI) {
      float correction = roundf(diff / (float)M_PI) * (float)M_PI;

      if (fabsf(correction) > 0.0f) {
        int start_smooth_idx = i - SMOOTH_CORRECTION_WINDOW + 1;
        if (start_smooth_idx < 1)
          start_smooth_idx = 1;
        int win_len = i - start_smooth_idx + 1;

        for (int j = start_smooth_idx; j <= i; j++) {
          corrected_phase_step[j] -= (correction / (float)win_len);
        }
        current_abs_phase -= correction;
        corrections_made++;
      }
    }
  }

  float *phase_corrected = malloc(length_N * sizeof(float));
  float *t_delay_corrected = malloc(length_N * sizeof(float));

  phase_corrected[0] = corrected_phase_step[0];
  t_delay_corrected[0] = 0.0f;

  for (int i = 1; i < length_N; i++) {
    phase_corrected[i] = phase_corrected[i - 1] + corrected_phase_step[i];
    float f = ((float)i / (float)FFT_SIZE) * (float)fs;
    t_delay_corrected[i] = -phase_corrected[i] / (2.0f * (float)M_PI * f);
  }

  // final calculation of tdoa
  float final_delay_sec = 0.0f;
  float total_weight = 0.0f;

  int s_bin = (int)roundf((PBDE_F_MIN * (float)FFT_SIZE) / (float)fs);
  int e_bin = (int)roundf((PBDE_F_MAX * (float)FFT_SIZE) / (float)fs);

  if (s_bin < 1)
    s_bin = 1;
  if (e_bin >= length_N)
    e_bin = length_N - 1;
  if (s_bin > e_bin) {
    s_bin = 1;
    e_bin = length_N - 1;
  }

  for (int i = s_bin; i <= e_bin; i++) {
    float normalized_weight = norm_weights[i] / max_cross_amplitude;
    final_delay_sec += t_delay_corrected[i] * normalized_weight;
    total_weight += normalized_weight;
  }

  if (total_weight > 0.0f) {
    final_delay_sec = final_delay_sec / total_weight;
  }

  res.delay_ms = final_delay_sec * 1000.0f;
  res.lag_samples = (int)roundf(final_delay_sec * (float)fs);

  // DATA PRINTER
  printf("\n---PBDE_LIN_REG_START---\n");
  printf("DELAY:");
  for (int i = 0; i < length_N; i++) {
    printf("%f,", t_delay_corrected[i] * 1000.0f);
  }
  printf("\nWEIGHTS:");
  for (int i = 0; i < length_N; i++) {
    printf("%f,", norm_weights[i] / max_cross_amplitude);
  }
  printf("\nPHASE:");
  for (int i = 0; i < length_N; i++) {
    printf("%f,", phase_corrected[i]);
  }
  printf("\nPHASE_STEP:");
  for (int i = 0; i < length_N; i++) {
    printf("%f,", p_step[i]);
  }
  printf("\n---PBDE_LIN_REG_END---\n");

  free(norm_weights);
  free(u_phase);
  free(p_step);
  free(corrected_phase_step);
  free(phase_corrected);
  free(t_delay_corrected);

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
  int M = (int)(0.85f * L);
  float beta = 12.0f;
  int transition_point = L - M;

  int hanning_len = 2 * (L - M);
  for (int n = 0; n < transition_point; n++) {
    float hann =
        0.5f *
        (1.0f - cosf(2.0f * (float)M_PI * (float)n / (float)hanning_len));
    w[n] = sqrtf(hann);
  }

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