#ifndef PEAK_DETECTOR_H
#define PEAK_DETECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum peak_det_state {
  PEAK_DET_OK = 0,
  PEAK_DET_ERR_CFG_UNINITIALIZED = -200,
  PEAK_DET_ERR_BUFFER_TOO_SMALL = -201,
  PEAK_DET_ERR_INVALID_ARG = -202
};

/** @brief Detekční prahy pro impulzní událost. */
struct median_detector_levels {
  int16_t det_level;
  int16_t det_rms;
  int16_t det_energy;
};

/** @brief Konfigurace detektoru. */
struct median_detector_cfg {
  uint8_t num_taps;
  uint16_t tap_size;
  struct median_detector_levels levels;
};

/** @brief Výsledek detekce po zpracování bloku. */
struct detector_result {
  bool hit;
  int peak_index;
};

// Forward declaration for opaque state
struct detector_state;

// Functions
/**
 * @brief Vrátí potřebnou velikost paměťového bloku pro stav podle konfigurace.
 *
 * @param cfg       konfigurace (nesmí být NULL)
 * @param out_size  výstup: velikost v bajtech, zarovnaná pro interní struktury
 * @return PEAK_DET_OK nebo chybový kód
 */
enum peak_det_state detector_state_size(const struct median_detector_cfg *cfg,
                                        size_t *out_size);

/**
 * @brief Inicializuje stav v uživatelem dodaném bufferu (bez malloc).
 *
 * @param mem       ukazatel na předalokovaný buffer
 * @param mem_size  velikost bufferu v bajtech
 * @param cfg       konfigurace
 * @param out       výstup: ukazatel na inicializovaný stav uvnitř bufferu
 * @return PEAK_DET_OK nebo chybový kód
 */
enum peak_det_state detector_init(void *mem, size_t mem_size,
                                  const struct median_detector_cfg *cfg,
                                  struct detector_state **out);

void detector_deinit(struct detector_state *s); // optional cleanup
void detector_reset(struct detector_state *s);  // reset execution

/**
 * @brief Online zpracování jednoho tapu (bloku) vzorků.
 *
 * @param s                  interní stav
 * @param block              vstupní vzorky délky tap_size
 * @param block_start_offset logický offset signálu (pro zpětné určení polohy)
 * @param out                výsledek detekce (může být NULL)
 * @return PEAK_DET_OK nebo chybový kód
 */
int detector_feed_block(struct detector_state *s, const int16_t *block,
                        int64_t block_start_offset,
                        struct detector_result *out);

#ifdef PEAK_DETECTOR_TESTING
/// Test-only helper pro injektování do medianu.
void peak_test_median_update(struct detector_state *s, uint16_t offset,
                             int16_t value, uint16_t tap_idx, uint32_t gen);
/// Test-only helper pro přečtení medianu na offsetu.
int16_t peak_test_median_value(struct detector_state *s, uint16_t offset);
/// Test-only helper pro přečtení RMS akumulátoru.
uint64_t peak_test_rms_acc(const struct detector_state *s);
#endif

#endif // PEAK_DETECTOR_H
