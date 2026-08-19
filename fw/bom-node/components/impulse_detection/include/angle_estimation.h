#ifndef ANGLE_ESTIMATION_H
#define ANGLE_ESTIMATION_H
#include <stdint.h>

#ifndef MIC_DISTANCE_METERS
#define MIC_DISTANCE_METERS 0.186f
#endif

#ifndef SPEED_OF_SOUND_MPS
#define SPEED_OF_SOUND_MPS 343.0f
#endif

#ifndef FFT_SIZE
#define FFT_SIZE 1024
#endif

#ifndef PBDE_F_MAX
#define PBDE_F_MAX 10000.0f
#endif

#ifndef PBDE_F_MIN
#define PBDE_F_MIN 500.0f
#endif

#define TOTAL_SAMPLES (FFT_SIZE * 2)

typedef struct {
  int lag_samples; // lag described by number of samples (+ or -)
  float delay_ms;  // lag described in milliseconds
} tdoa_estimation;

float calculate_aoa(tdoa_estimation tdoa);

void fft_init();

tdoa_estimation cross_corr(const int16_t *signal1, const int16_t *signal2,
                           int length, int fs);

tdoa_estimation cross_corr_parabolic(const int16_t *signal1,
                                     const int16_t *signal2, int length,
                                     int fs);

tdoa_estimation pbde_basic(const int16_t *signal1, const int16_t *signal2,
                           int length, int fs);

tdoa_estimation pbde_lin_reg(const int16_t *signal1, const int16_t *signal2,
                             int length, int fs);

void generate_root_hann_kaiser_window(float *w, int L);

#endif