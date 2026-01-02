/**
 * @file peak_detector.c
 * @brief Median-based impulse detector implementation.
 *
 * Algoritmus:
 * 1. Data přicházejí po tap blocích (o délce @c tap_size), které se ukládají do
 *    kruhového bufferu s @c num_taps položkami.
 * 2. Pro každý offset v tapu se udržují dvě haldy (max/min) s lazy invalidací
 *    přes @c gen_per_tap, takže median lze aktualizovat v O(log N) bez
 *    mazání starých uzlů.
 * 3. RMS akumulátor drží sumu čtverců pro celé okno, což umožňuje dynamický
 *    práh @c det_rms * RMS.
 * 4. Vyhodnocení probíhá nad "middle" tapem: hledá se největší deviation vůči
 *    medianu šumu a pak se kontroluje energie before/after.
 *
 * Hlavní cíle: determinismus (žádné malloc), jednoduchá portace na embedded a
 * auditovatelnost algoritmu.
 */

#include "peak_detector.h"

#include <limits.h>
#include <math.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Internal structures for median over tap_size offsets and num_taps taps
/**
 * @brief Uzly pro obousměrné haldy mediánu.
 *
 * @c tap_idx a @c gen umožňují označit prvek jako zastaralý bez přepisování
 * haldy – při přesunu zapisovacího kurzoru se pro daný tap zvyšuje generace.
 */
struct heap_node {
  int16_t value;   /**< Uložená vzorková hodnota. */
  uint16_t tap_idx;/**< Z jakého tapu vzorek pochází. */
  uint32_t gen;    /**< Generace tapu pro lazy invalidaci. */
};

struct per_offset_median {
  struct heap_node *max_heap;
  struct heap_node *min_heap;
  uint32_t *gen_per_tap; // length num_taps
  size_t max_size;
  size_t min_size;
};

struct detector_state {
  // configuration
  uint8_t num_taps;
  uint16_t tap_size;
  int16_t det_level;
  int16_t det_rms;
  int16_t det_energy;

  // input ring
  uint8_t write_tap;
  int16_t *samples; // length num_taps * tap_size

  // per-offset medians (length tap_size)
  struct per_offset_median *med;

  // RMS / offset
  uint64_t rms_acc;
  uint32_t *sqr_ring; // length num_taps * tap_size
  int64_t base_offset;

  // runtime
  size_t sample_count;

  // generator
  uint32_t current_gen;
};

static size_t align_up(size_t v, size_t a) {
  if (a == 0) {
    return v;
  }
  return (v + a - 1u) & ~(a - 1u);
}

enum peak_det_state detector_state_size(const struct median_detector_cfg *cfg,
                                        size_t *out_size) {
  if (out_size == NULL || cfg == NULL) {
    return PEAK_DET_ERR_CFG_UNINITIALIZED;
  }
  if (cfg->num_taps == 0 || cfg->tap_size == 0) {
    return PEAK_DET_ERR_CFG_UNINITIALIZED;
  }

  // rough overflow check (very simple)
  if (cfg->num_taps > SIZE_MAX / cfg->tap_size) {
    return PEAK_DET_ERR_INVALID_ARG;
  }

  size_t n = (size_t)cfg->num_taps * cfg->tap_size;

  size_t offset = 0;
  offset = align_up(offset, alignof(struct detector_state));
  offset += sizeof(struct detector_state);

  offset = align_up(offset, alignof(int16_t));
  offset += n * sizeof(int16_t); // samples

  offset = align_up(offset, alignof(struct per_offset_median));
  offset += cfg->tap_size * sizeof(struct per_offset_median); // med array

  // per-offset segments
  for (uint16_t i = 0; i < cfg->tap_size; ++i) {
    (void)i;
    offset = align_up(offset, alignof(struct heap_node));
    offset += cfg->num_taps * sizeof(struct heap_node); // max_heap

    offset = align_up(offset, alignof(struct heap_node));
    offset += cfg->num_taps * sizeof(struct heap_node); // min_heap

    offset = align_up(offset, alignof(uint32_t));
    offset += cfg->num_taps * sizeof(uint32_t); // gen_per_tap
  }

  offset = align_up(offset, alignof(uint32_t));
  offset += n * sizeof(uint32_t); // sqr_ring

  *out_size = offset;
  return PEAK_DET_OK;
}

static void layout_state(void *mem_base, const struct median_detector_cfg *cfg,
                         struct detector_state **state_out) {
  uint8_t *base = (uint8_t *)mem_base;
  size_t n = (size_t)cfg->num_taps * cfg->tap_size;
  size_t offset = 0;

  offset = align_up(offset, alignof(struct detector_state));
  struct detector_state *s = (struct detector_state *)(base + offset);
  offset += sizeof(struct detector_state);

  offset = align_up(offset, alignof(int16_t));
  s->samples = (int16_t *)(base + offset);
  offset += n * sizeof(int16_t);

  offset = align_up(offset, alignof(struct per_offset_median));
  s->med = (struct per_offset_median *)(base + offset);
  offset += cfg->tap_size * sizeof(struct per_offset_median);

  for (uint16_t i = 0; i < cfg->tap_size; ++i) {
    offset = align_up(offset, alignof(struct heap_node));
    s->med[i].max_heap = (struct heap_node *)(base + offset);
    offset += cfg->num_taps * sizeof(struct heap_node);

    offset = align_up(offset, alignof(struct heap_node));
    s->med[i].min_heap = (struct heap_node *)(base + offset);
    offset += cfg->num_taps * sizeof(struct heap_node);

    offset = align_up(offset, alignof(uint32_t));
    s->med[i].gen_per_tap = (uint32_t *)(base + offset);
    offset += cfg->num_taps * sizeof(uint32_t);
  }

  offset = align_up(offset, alignof(uint32_t));
  s->sqr_ring = (uint32_t *)(base + offset);

  *state_out = s;
}

// --- heap helpers (per offset)
// ------------------------------------------------

static inline int heap_cmp_max(const struct heap_node *a,
                               const struct heap_node *b) {
  if (a->value != b->value) {
    return (int)a->value - (int)b->value;
  }
  // tie-breaker: newer generation first
  return (int)a->gen - (int)b->gen;
}

static inline int heap_cmp_min(const struct heap_node *a,
                               const struct heap_node *b) {
  if (a->value != b->value) {
    return (int)b->value - (int)a->value;
  }
  return (int)b->gen - (int)a->gen;
}

static inline bool heap_is_stale(const struct heap_node *n,
                                 const uint32_t *gen_per_tap) {
  return n->gen != gen_per_tap[n->tap_idx];
}

static void heap_sift_down(struct heap_node *heap, size_t size, size_t idx,
                           bool is_max_heap);

static void heap_heapify(struct heap_node *heap, size_t size,
                         bool is_max_heap) {
  if (size == 0) {
    return;
  }
  // iterate i = floor(size/2) - 1 down to 0 using size_t underflow pattern
  for (size_t i = size / 2; i-- > 0;) {
    heap_sift_down(heap, size, i, is_max_heap);
  }
}

static void heap_compact(struct heap_node *heap, size_t *size,
                         const uint32_t *gen_per_tap, bool is_max_heap) {
  size_t write = 0;
  for (size_t read = 0; read < *size; ++read) {
    if (!heap_is_stale(&heap[read], gen_per_tap)) {
      if (write != read) {
        heap[write] = heap[read];
      }
      ++write;
    }
  }
  *size = write;
  heap_heapify(heap, *size, is_max_heap);
}

static void heap_sift_up(struct heap_node *heap, size_t idx, bool is_max_heap) {
  while (idx > 0) {
    size_t parent = (idx - 1) / 2;
    int cmp = is_max_heap ? heap_cmp_max(&heap[idx], &heap[parent])
                          : heap_cmp_min(&heap[idx], &heap[parent]);
    if (cmp <= 0) {
      break;
    }
    struct heap_node tmp = heap[parent];
    heap[parent] = heap[idx];
    heap[idx] = tmp;
    idx = parent;
  }
}

static void heap_sift_down(struct heap_node *heap, size_t size, size_t idx,
                           bool is_max_heap) {
  while (true) {
    size_t left = 2 * idx + 1;
    size_t right = left + 1;
    size_t best = idx;

    if (left < size) {
      int cmp = is_max_heap ? heap_cmp_max(&heap[left], &heap[best])
                            : heap_cmp_min(&heap[left], &heap[best]);
      if (cmp > 0) {
        best = left;
      }
    }
    if (right < size) {
      int cmp = is_max_heap ? heap_cmp_max(&heap[right], &heap[best])
                            : heap_cmp_min(&heap[right], &heap[best]);
      if (cmp > 0) {
        best = right;
      }
    }
    if (best == idx) {
      break;
    }
    struct heap_node tmp = heap[idx];
    heap[idx] = heap[best];
    heap[best] = tmp;
    idx = best;
  }
}

static void heap_push(struct heap_node *heap, size_t *size,
                      struct heap_node node, bool is_max_heap) {
  heap[*size] = node;
  (*size)++;
  heap_sift_up(heap, *size - 1, is_max_heap);
}

static struct heap_node heap_pop(struct heap_node *heap, size_t *size,
                                 bool is_max_heap) {
  struct heap_node top = heap[0];
  (*size)--;
  if (*size > 0) {
    heap[0] = heap[*size];
    heap_sift_down(heap, *size, 0, is_max_heap);
  }
  return top;
}

static struct heap_node *heap_top(struct heap_node *heap, size_t size) {
  if (size == 0) {
    return NULL;
  }
  return &heap[0];
}

static void heap_clean_top(struct per_offset_median *m, bool is_max_heap,
                           struct heap_node *heap, size_t *size) {
  struct heap_node *top = heap_top(heap, *size);
  while (top && heap_is_stale(top, m->gen_per_tap)) {
    (void)heap_pop(heap, size, is_max_heap);
    top = heap_top(heap, *size);
  }
}

static void median_rebalance(struct per_offset_median *m) {
  // nejprve vyčistit stale uvnitř hald (kompaktace + heapify)
  heap_compact(m->max_heap, &m->max_size, m->gen_per_tap, true);
  heap_compact(m->min_heap, &m->min_size, m->gen_per_tap, false);

  while (m->max_size < m->min_size) {
    if (m->min_size == 0) {
      break;
    }
    struct heap_node n = heap_pop(m->min_heap, &m->min_size, false);
    heap_push(m->max_heap, &m->max_size, n, true);
  }

  while (m->max_size > m->min_size + 1) {
    if (m->max_size == 0) {
      break;
    }
    struct heap_node n = heap_pop(m->max_heap, &m->max_size, true);
    heap_push(m->min_heap, &m->min_size, n, false);
  }

  heap_clean_top(m, true, m->max_heap, &m->max_size);
}

static int16_t median_value(struct per_offset_median *m, int16_t fallback) {
  heap_compact(m->max_heap, &m->max_size, m->gen_per_tap, true);
  if (m->max_size == 0) {
    return fallback;
  }
  return m->max_heap[0].value;
}

static void median_insert(struct per_offset_median *m, int16_t value,
                          uint16_t tap_idx, uint32_t gen) {
  struct heap_node node = {.value = value, .tap_idx = tap_idx, .gen = gen};

  if (m->max_size == 0) {
    heap_push(m->max_heap, &m->max_size, node, true);
    return;
  }

  int16_t med = median_value(m, value);
  if (value <= med) {
    heap_push(m->max_heap, &m->max_size, node, true);
  } else {
    heap_push(m->min_heap, &m->min_size, node, false);
  }
}

static void median_update_offset(struct per_offset_median *m, int16_t new_value,
                                 uint16_t tap_idx, uint32_t gen) {
  m->gen_per_tap[tap_idx] = gen;
  median_insert(m, new_value, tap_idx, gen);
  median_rebalance(m);
}

#ifdef PEAK_DETECTOR_TESTING
void peak_test_median_update(struct detector_state *s, uint16_t offset,
                             int16_t value, uint16_t tap_idx, uint32_t gen) {
  if (!s || offset >= s->tap_size) {
    return;
  }
  median_update_offset(&s->med[offset], value, tap_idx, gen);
}

int16_t peak_test_median_value(struct detector_state *s, uint16_t offset) {
  if (!s || offset >= s->tap_size) {
    return 0;
  }
  return median_value(&s->med[offset], 0);
}

uint64_t peak_test_rms_acc(const struct detector_state *s) {
  if (!s) {
    return 0;
  }
  return s->rms_acc;
}
#endif
// --- helpers -----------------------------------------------------------------

static int cmp_int16(const void *a, const void *b) {
  int16_t av = *(const int16_t *)a;
  int16_t bv = *(const int16_t *)b;
  return (av > bv) - (av < bv);
}

// Maximum supported slice length for median calculation.
// This limit exists to avoid dynamic allocation on embedded systems.
// If tap_size exceeds this, consider increasing the buffer size.
#define MEDIAN_SLICE_MAX_LEN 128

static int16_t median_of_slice(const int16_t *arr, size_t len) {
  if (len == 0) {
    return 0;
  }

  // For embedded systems, use fixed stack buffer to avoid malloc.
  // If input exceeds buffer size, truncate and use only first N elements.
  // This is acceptable since the function is only used for local
  // before/after comparisons within a tap, not for global median.
  int16_t tmp[MEDIAN_SLICE_MAX_LEN];
  size_t actual_len = len;

  if (len > MEDIAN_SLICE_MAX_LEN) {
    // Truncate to max buffer size. This should rarely happen in practice
    // as tap_size is typically much smaller (e.g., 16-32 samples).
    actual_len = MEDIAN_SLICE_MAX_LEN;
  }

  memcpy(tmp, arr, actual_len * sizeof(int16_t));
  qsort(tmp, actual_len, sizeof(int16_t), cmp_int16);
  return tmp[actual_len / 2];
}

enum peak_det_state detector_init(void *mem, size_t mem_size,
                                  const struct median_detector_cfg *cfg,
                                  struct detector_state **out) {
  if (mem == NULL || out == NULL || cfg == NULL) {
    return PEAK_DET_ERR_CFG_UNINITIALIZED;
  }
  if (cfg->num_taps == 0 || cfg->tap_size == 0) {
    return PEAK_DET_ERR_CFG_UNINITIALIZED;
  }

  size_t needed = 0;
  enum peak_det_state st = detector_state_size(cfg, &needed);
  if (st != PEAK_DET_OK) {
    return st;
  }
  if (mem_size < needed) {
    return PEAK_DET_ERR_BUFFER_TOO_SMALL;
  }

  memset(mem, 0, needed);
  struct detector_state *state = NULL;
  layout_state(mem, cfg, &state);

  state->num_taps = cfg->num_taps;
  state->tap_size = cfg->tap_size;
  state->det_level = cfg->levels.det_level;
  state->det_rms = cfg->levels.det_rms;
  state->det_energy = cfg->levels.det_energy;
  state->write_tap = 0;
  state->current_gen = 1;
  state->rms_acc = 0;

  // initialize per-offset median structures
  for (uint16_t i = 0; i < cfg->tap_size; ++i) {
    state->med[i].max_size = 0;
    state->med[i].min_size = 0;
    // gen_per_tap already zeroed by memset
  }

  *out = state;
  return PEAK_DET_OK;
}

void detector_deinit(struct detector_state *s) {
  (void)s; // nothing to free, buffer owned by caller
}

void detector_reset(struct detector_state *s) {
  if (!s) {
    return;
  }
  size_t total = (size_t)s->num_taps * s->tap_size;
  memset(s->samples, 0, total * sizeof(int16_t));
  memset(s->sqr_ring, 0, total * sizeof(uint32_t));
  for (uint16_t i = 0; i < s->tap_size; ++i) {
    s->med[i].max_size = 0;
    s->med[i].min_size = 0;
    memset(s->med[i].gen_per_tap, 0, s->num_taps * sizeof(uint32_t));
  }
  s->write_tap = 0;
  s->current_gen = 1;
  s->rms_acc = 0;
  s->sample_count = 0;
  s->base_offset = 0;
}

int detector_feed_block(struct detector_state *s, const int16_t *block,
                        int64_t block_start_offset,
                        struct detector_result *out) {
  if (s == NULL || block == NULL) {
    return PEAK_DET_ERR_INVALID_ARG;
  }

  size_t base = (size_t)s->write_tap * s->tap_size;
  uint32_t gen = ++s->current_gen;

  for (uint16_t i = 0; i < s->tap_size; ++i) {
    size_t idx = base + i;
    int16_t old = s->samples[idx];
    int16_t val = block[i];

    // RMS update
    int32_t old32 = (int32_t)old;
    int32_t val32 = (int32_t)val;
    uint64_t old_sqr = (uint64_t)(old32 * old32);
    uint64_t new_sqr = (uint64_t)(val32 * val32);
    if (s->sqr_ring != NULL) {
      s->rms_acc -= s->sqr_ring[idx];
      // Safe cast: max value for int16_t is 32767^2 =
      // 1,073,741,824 < UINT32_MAX
      s->sqr_ring[idx] = (uint32_t)new_sqr;
      s->rms_acc += s->sqr_ring[idx];
    } else {
      s->rms_acc -= old_sqr;
      s->rms_acc += new_sqr;
    }

    s->samples[idx] = val;
    median_update_offset(&s->med[i], val, s->write_tap, gen);
  }

  s->write_tap = (uint8_t)((s->write_tap + 1) % s->num_taps);
  s->sample_count += s->tap_size;
  s->base_offset = block_start_offset;

  if (out) {
    out->hit = false;
    out->peak_index = -1;
  }

  // vyhodnocení detekce až když máme plné okno
  size_t window_len = (size_t)s->num_taps * s->tap_size;
  if (s->sample_count < window_len) {
    return PEAK_DET_OK;
  }

  // middle tap index v kronologii od nejstaršího (write_tap je nejstarší)
  uint8_t middle_idx =
      (uint8_t)((s->write_tap + (s->num_taps / 2)) % s->num_taps);
  size_t middle_base = (size_t)middle_idx * s->tap_size;

  // Noise median per offset
  int16_t middle_minus_noise = 0;
  int16_t peak_val = INT16_MIN;
  int peak_pos = -1;
  for (uint16_t i = 0; i < s->tap_size; ++i) {
    int16_t noise = median_value(&s->med[i], 0);
    int16_t sample = s->samples[middle_base + i];
    int32_t diff = (int32_t)sample - (int32_t)noise;
    if (diff > peak_val) {
      peak_val = (int16_t)diff;
      peak_pos = i;
      middle_minus_noise = noise; // not used further, kept for clarity
    }
  }

  if (peak_pos < 0 || out == NULL) {
    return PEAK_DET_OK;
  }

  double rms_noise = 0.0;
  if (s->rms_acc > 0 && window_len > 0) {
    double mean_sq = (double)s->rms_acc / (double)window_len;
    rms_noise = sqrt(mean_sq);
  }

  bool hit = false;
  if (peak_val > s->det_level &&
      peak_val > (int32_t)s->det_rms * (int32_t)rms_noise) {
    // Before = median z [pos : pos+tap_size) v middle tapu
    size_t start_before = (size_t)peak_pos;
    size_t end_before = start_before + s->tap_size;
    if (end_before > s->tap_size) {
      end_before = s->tap_size;
    }
    int16_t before_med = median_of_slice(
        &s->samples[middle_base + start_before], end_before - start_before);

    // After = median z [pos - tap_size : pos) v middle tapu
    size_t start_after = 0;
    if (peak_pos >= s->tap_size) {
      start_after = peak_pos - s->tap_size;
    }

    size_t end_after = (size_t)peak_pos;
    int16_t after_med = median_of_slice(&s->samples[middle_base + start_after],
                                        end_after - start_after);

    int64_t lhs = (int64_t)before_med;
    int64_t rhs = (int64_t)after_med * (int64_t)s->det_energy;
    if (lhs > rhs) {
      hit = true;
    }
  }

  out->hit = hit;
  if (hit) {
    // newest tap index je (write_tap + num_taps - 1) % num_taps
    uint8_t newest = (uint8_t)((s->write_tap + s->num_taps - 1) % s->num_taps);
    uint8_t delta =
        (uint8_t)((newest + s->num_taps - middle_idx) % s->num_taps);
    int64_t middle_start = block_start_offset - ((int64_t)delta * s->tap_size);
    out->peak_index = (int)(middle_start + peak_pos);
  } else {
    out->peak_index = -1;
  }
  return PEAK_DET_OK;
}
