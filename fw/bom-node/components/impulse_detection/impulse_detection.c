#include "impulse_detection.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* --- INTERNÍ KONFIGURACE --- */

static impulse_detection_params_t g_det = {
    .num_taps = 25,      // odpovídá DP
    .tap_size = 20,      // délka jednoho tapu
    .det_rms = 15.0f,    // násobek RMS
    .det_level = 100000, // minimální hodnota – lze doladit
    .det_energy = 0.1f   // poměr energií B/A
};

void impulse_detection_init(const impulse_detection_params_t *params) {
  if (params) {
    g_det = *params;
  }
}

/* --- INTERNÍ POMOCNÉ FUNKCE --- */

static int cmp_u32(const void *a, const void *b) {
  uint32_t va = *(const uint32_t *)a;
  uint32_t vb = *(const uint32_t *)b;
  if (va < vb)
    return -1;
  if (va > vb)
    return 1;
  return 0;
}

static uint32_t median_u32(uint32_t *data, int n) {
  qsort(data, n, sizeof(uint32_t), cmp_u32);
  if (n & 1)
    return data[n / 2];
  else
    return (data[n / 2 - 1] + data[n / 2]) / 2;
}

/**
 * MedianFilter(P):
 * - P je pole výkonu S^2 (num_taps * tap_size)
 * - noise_out má délku tap_size
 */
static void median_filter(const uint32_t *P, int num_taps, int tap_size,
                          uint32_t *noise_out) {
  uint32_t *tmp = malloc(num_taps * sizeof(uint32_t));
  if (!tmp)
    return;

  for (int i = 0; i < tap_size; ++i) {
    for (int t = 0; t < num_taps; ++t)
      tmp[t] = P[t * tap_size + i];
    noise_out[i] = median_u32(tmp, num_taps);
  }

  free(tmp);
}

/* --- HLAVNÍ FUNKCE DETEKCE --- */

bool impulse_detect(const int16_t *samples, int num_samples) {
  const int num_taps = g_det.num_taps;
  const int tap_size = g_det.tap_size;
  const int used_samples = num_taps * tap_size;

  if (num_samples < used_samples) {
    // příliš málo dat
    return false;
  }

  // 1) Výkon signálu
  uint32_t *P = malloc(sizeof(uint32_t) * used_samples);
  if (!P)
    return false;

  for (int i = 0; i < used_samples; ++i) {
    int32_t s = samples[i];
    P[i] = (uint32_t)(s * s);
  }

  // 2) Medianový filtr
  uint32_t *Noise = malloc(sizeof(uint32_t) * tap_size);
  uint32_t *Middle = malloc(sizeof(uint32_t) * tap_size);
  if (!Noise || !Middle) {
    free(P);
    free(Noise);
    free(Middle);
    return false;
  }

  median_filter(P, num_taps, tap_size, Noise);

  // 3) Střední tap minus šum
  int mid = num_taps / 2;
  for (int i = 0; i < tap_size; ++i) {
    uint32_t val = P[mid * tap_size + i];
    Middle[i] = (val > Noise[i]) ? (val - Noise[i]) : 0;
  }

  // 4) Najdi maximum
  uint32_t Val = 0;
  int pos_in_mid = 0;
  for (int i = 0; i < tap_size; ++i) {
    if (Middle[i] > Val) {
      Val = Middle[i];
      pos_in_mid = i;
    }
  }
  int Pos = mid * tap_size + pos_in_mid;

  // 5) RMS šumu
  double sum_sq = 0.0;
  for (int i = 0; i < tap_size; ++i)
    sum_sq += (double)Noise[i] * (double)Noise[i];
  double rms_noise = sqrt(sum_sq / tap_size);

  bool result = false;

  // 6) 1. podmínka
  if (Val > g_det.det_level && Val > (uint32_t)(g_det.det_rms * rms_noise)) {
    // 7) 2. podmínka – energie před/po
    if (Pos - tap_size >= 0 && Pos + tap_size <= used_samples) {
      uint32_t *tmp = malloc(sizeof(uint32_t) * tap_size);
      if (!tmp) {
        free(P);
        free(Noise);
        free(Middle);
        return false;
      }

      memcpy(tmp, &P[Pos - tap_size], tap_size * sizeof(uint32_t));
      uint32_t A = median_u32(tmp, tap_size);

      memcpy(tmp, &P[Pos], tap_size * sizeof(uint32_t));
      uint32_t B = median_u32(tmp, tap_size);

      free(tmp);

      if ((double)B > (double)A * g_det.det_energy)
        result = true;
    }
  }

  free(P);
  free(Noise);
  free(Middle);
  return result;
}
