#ifndef ANGLE_ESTIMATION_H
#define ANGLE_ESTIMATION_H
#include <stdint.h>

#ifndef MIC_DISTANCE_METERS
#define MIC_DISTANCE_METERS 0.175f
#endif

#ifndef SPEED_OF_SOUND_MPS
#define SPEED_OF_SOUND_MPS 340.0f
#endif

#ifndef FFT_SIZE
#define FFT_SIZE 128
#endif

#define TOTAL_SAMPLES (FFT_SIZE * 2)

typedef struct {
  int lag_samples; // lag described by number of samples (+ or -)
  float delay_ms;  // lag described in milliseconds
} tdoa_estimation;

tdoa_estimation cross_corr_tdoa(const int16_t *signal1, const int16_t *signal2,
                                int length, int fs);

float calculate_aoa(tdoa_estimation tdoa);

void pbde_estimator_init();
tdoa_estimation pbde_tdoa(const int16_t *signal1, const int16_t *signal2,
                          int length, int fs);
void generate_root_hann_kaiser_window(float *w, int L);

#endif