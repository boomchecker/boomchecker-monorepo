#include "impulse_detection.h"
#include <math.h>
#include <string.h>

static uint16_t remove_value_sorted(uint32_t *arr, uint16_t n, uint32_t val) {
  uint16_t idx = n;
  for (uint16_t i = 0; i < n; i++) {
    if (arr[i] == val) {
      idx = i;
      break;
    }
  }

  if (idx == n) {
    if (n > 0)
      return n - 1;
    return 0;
  }

  for (uint16_t i = idx; i + 1 < n; i++) {
    arr[i] = arr[i + 1];
  }
  return n - 1;
}

static uint16_t insert_value_sorted(uint32_t *arr, uint16_t n,
                                    uint16_t capacity, uint32_t val) {
  if (n >= capacity) {
    return n;
  }

  uint16_t pos = 0;
  while (pos < n && arr[pos] <= val)
    pos++;

  for (int i = (int)n; i > (int)pos; i--) {
    arr[i] = arr[i - 1];
  }
  arr[pos] = val;
  return n + 1;
}

static inline uint16_t oldest_index(const impulse_detector *det) {
  if (det->count < TAP_COUNT)
    return 0;
  return (uint16_t)((det->head + 1) % TAP_COUNT);
}

static inline uint16_t tap_index_by_age_from_oldest(const impulse_detector *det,
                                                    uint16_t age) {
  uint16_t o = oldest_index(det);
  return (uint16_t)((o + age) % TAP_COUNT);
}

static inline uint16_t middle_tap_index(const impulse_detector *det) {
  uint16_t mid_age = (uint16_t)(TAP_COUNT / 2);
  return tap_index_by_age_from_oldest(det, mid_age);
}

static inline uint32_t get_P_global(const impulse_detector *det, int32_t g) {
  uint16_t age = (uint16_t)(g / TAP_SIZE);
  uint16_t off = (uint16_t)(g % TAP_SIZE);
  uint16_t tix = tap_index_by_age_from_oldest(det, age);
  return det->taps[tix][off];
}

void impulse_detection_init(impulse_detector *det) {
  memset(det, 0, sizeof(*det));
  det->head = 0;
  det->count = 0;
}

void impulse_add_tap(impulse_detector *det, const int16_t *samples) {
  uint16_t write_idx;
  if (det->count == 0) {
    write_idx = 0;
  } else {
    write_idx = (uint16_t)((det->head + 1) % TAP_COUNT);
  }

  bool full = (det->count == TAP_COUNT);

  for (uint16_t i = 0; i < TAP_SIZE; i++) {

    uint32_t old_val = det->taps[write_idx][i];
    int32_t s = samples[i];
    uint32_t new_val = (uint32_t)(s * s);

    det->taps[write_idx][i] = new_val;

    uint32_t *col = det->sorted_cols[i];

    if (!full) {
      col[det->count] = new_val;
      uint16_t n = det->count;
      int j = (int)n - 1;
      while (j >= 0 && col[j] > new_val) {
        col[j + 1] = col[j];
        j--;
      }
      col[j + 1] = new_val;
    } else {
      uint16_t n = TAP_COUNT;
      n = remove_value_sorted(col, n, old_val);
      (void)insert_value_sorted(col, n, TAP_COUNT, new_val);
    }
  }

  det->head = write_idx;
  if (det->count < TAP_COUNT)
    det->count++;
}

static inline uint32_t noise_median_at(const impulse_detector *det,
                                       uint16_t i) {
  return det->sorted_cols[i][TAP_COUNT / 2];
}

static void sort_insertion_u32(uint32_t *arr, uint16_t n) {
  for (uint16_t i = 1; i < n; i++) {
    uint32_t key = arr[i];
    int j = (int)i - 1;
    while (j >= 0 && arr[j] > key) {
      arr[j + 1] = arr[j];
      j--;
    }
    arr[j + 1] = key;
  }
}

static uint32_t median_u32(uint32_t *arr, uint16_t n) {
  if (n == 0)
    return 0;
  sort_insertion_u32(arr, n);
  return arr[n / 2];
}

static uint16_t gather_window(const impulse_detector *det, int32_t start_g,
                              int32_t end_g, uint32_t *out) {
  uint16_t n = 0;
  for (int32_t g = start_g; g < end_g; g++) {
    out[n++] = get_P_global(det, g);
  }
  return n;
}

bool impulse_run_detection(impulse_detector *det) {
  if (det->count < TAP_COUNT) {
    return false;
  }

  uint16_t mid_idx = middle_tap_index(det);
  const uint32_t *mid_tap = det->taps[mid_idx];

  uint64_t sum_noise_sq = 0;

  uint32_t val = 0;
  int32_t pos = -1;
  uint32_t noise[TAP_SIZE];

  for (uint16_t i = 0; i < TAP_SIZE; i++) {
    noise[i] = noise_median_at(det, i);

    uint32_t diff = (mid_tap[i] > noise[i]) ? (mid_tap[i] - noise[i]) : 0;
    if (diff > val) {
      val = diff;
      pos = i;
    }
  }

  if (pos < 0)
    return false;

  // first criterion
  if (!(val > DET_LEVEL)) {
    return false;
  }

  for (uint16_t i = 0; i < TAP_SIZE; i++) {
    sum_noise_sq += (uint64_t)noise[i] * (uint64_t)noise[i];
  }

  float rms_noise = sqrtf((float)sum_noise_sq / (float)TAP_SIZE);

  // second criterion
  if ((float)val <= DET_RMS * rms_noise) {
    return false;
  }

  uint16_t mid_age = (uint16_t)(TAP_COUNT / 2);
  int32_t global_pos = (int32_t)mid_age * (int32_t)TAP_SIZE + pos;

  uint32_t bufA[TAP_SIZE];
  uint32_t bufB[TAP_SIZE];

  uint16_t lenA =
      gather_window(det, global_pos - (int32_t)TAP_SIZE, global_pos, bufA);

  uint16_t lenB =
      gather_window(det, global_pos, global_pos + (int32_t)TAP_SIZE, bufB);

  uint32_t medA = median_u32(bufA, lenA);
  uint32_t medB = median_u32(bufB, lenB);

  // third criterion
  if ((float)medB > (float)medA * DET_ENERGY) {
    return true;
  }

  return false;
}
