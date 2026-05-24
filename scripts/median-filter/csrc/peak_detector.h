#ifndef PEAK_DETECTOR_H
#define PEAK_DETECTOR_H

/**
 * @file peak_detector.h
 * @brief Median-based impulse detector public API.
 *
 * Detektor pracuje nad posuvným oknem o velikosti `num_taps * tap_size` a pro
 * každý offset udržuje medián napříč všemi tapy. Nad "middle" tapem hledá
 * odchylku od odhadovaného šumu, provádí RMS kontrolu a energetický test před
 * a po piku. Kód je napsán tak, aby běžel i v prostředí bez heapu – stav je
 * alokován volajícím.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Stavový kód vrácený API detektoru.
 */
enum peak_det_state {
  PEAK_DET_OK = 0,                         /**< Úspěch. */
  PEAK_DET_ERR_CFG_UNINITIALIZED = -200,   /**< Konfigurace nebo výstupní ukazatel nebyl předán. */
  PEAK_DET_ERR_BUFFER_TOO_SMALL = -201,    /**< Uživatel dodal příliš malý buffer pro stav. */
  PEAK_DET_ERR_INVALID_ARG = -202          /**< Neplatný vstup (např. nulové parametry). */
};

/**
 * @brief Detekční prahy pro impulzní událost.
 *
 * - @c det_level: pevný práh na maximální deviation vůči mediánu šumu.
 * - @c det_rms: násobek RMS šumu (odvozen z celého okna) pro dynamický práh.
 * - @c det_energy: poměr energie před / po piku, vyšší hodnoty zvyšují
 *   odolnost proti falešným poplachům.
 */
struct median_detector_levels {
  int16_t det_level;
  int16_t det_rms;
  int16_t det_energy;
};

/**
 * @brief Konfigurace detektoru.
 *
 * Detektor čte bloky @c tap_size vzorků (tap) do kruhového bufferu délky
 * @c num_taps tapů. Pro každý offset přes všechny tapy se udržuje medián
 * pomocí dvou hald. Z těchto mediánů se vyhodnocuje deviation v prostředním
 * tapu.
 */
struct median_detector_cfg {
  uint8_t num_taps;                    /**< Počet tapů v okně (>=2). */
  uint16_t tap_size;                   /**< Délka jednoho tapu. */
  struct median_detector_levels levels;/**< Nastavení prahů detekce. */
};

/**
 * @brief Výsledek detekce po zpracování bloku.
 */
struct detector_result {
  bool hit;       /**< True, pokud byl nalezen platný pík. */
  int peak_index; /**< Absolutní index piku v nahrávce, nebo -1. */
};

// Forward declaration for opaque state
struct detector_state;

// Functions
/**
 * @brief Vrátí potřebnou velikost paměťového bloku pro stav podle konfigurace.
 *
 * Funkce nealokuje paměť – pouze spočítá minimální velikost bufferu, který
 * musí volající poskytnout při @ref detector_init.
 *
 * @param cfg       Konfigurace detektoru (nesmí být NULL).
 * @param out_size  Výstup: požadovaná velikost v bajtech, zarovnaná pro interní
 *                  struktury.
 * @return PEAK_DET_OK nebo chybový kód.
 */
enum peak_det_state detector_state_size(const struct median_detector_cfg *cfg,
                                        size_t *out_size);

/**
 * @brief Inicializuje stav v uživatelem dodaném bufferu (bez malloc).
 *
 * Inicializace nastaví kruhové buffery na nulu, zapíše parametry z
 * @p cfg a připraví struktury pro výpočet mediánu (max/min heap per offset) a
 * RMS akumulátor pro celé okno.
 *
 * @param mem       Ukazatel na předalokovaný buffer.
 * @param mem_size  Velikost bufferu v bajtech.
 * @param cfg       Konfigurace detektoru.
 * @param out       Výstup: ukazatel na inicializovaný stav uvnitř bufferu.
 * @return PEAK_DET_OK nebo chybový kód.
 */
enum peak_det_state detector_init(void *mem, size_t mem_size,
                                  const struct median_detector_cfg *cfg,
                                  struct detector_state **out);

void detector_deinit(struct detector_state *s); // optional cleanup
void detector_reset(struct detector_state *s);  // reset execution

/**
 * @brief Online zpracování jednoho tapu (bloku) vzorků.
 *
 * Postup:
 * 1. Aktualizuje per-offset mediány s lazy invalidací starých generací.
 * 2. Udržuje RMS akumulátor přes celé okno.
 * 3. Jakmile je okno naplněné, vyhodnotí deviation v "middle" tapu a provede
 *    energetický test before/after.
 *
 * @param s                  Interní stav.
 * @param block              Vstupní vzorky délky tap_size.
 * @param block_start_offset Logický offset signálu (pro zpětné určení polohy).
 * @param out                Výsledek detekce (může být NULL, pak se ignoruje).
 * @return PEAK_DET_OK nebo chybový kód.
 */
int detector_feed_block(struct detector_state *s, const int16_t *block,
                        int64_t block_start_offset,
                        struct detector_result *out);

/**
 * @brief Offline detekce nad celou nahrávkou (16bit).
 *
 * @param samples   vstupní pole vzorků
 * @param n         počet vzorků
 * @param cfg       konfigurace
 * @param positions výstupní pole pro nalezené pozice (absolutní indexy)
 * @param capacity  kapacita pole positions
 * @return počet detekovaných pozic (>=0) nebo chybový kód (<0)
 */
int detect_recording_i16(const int16_t *samples, size_t n,
                         const struct median_detector_cfg *cfg, int *positions,
                         size_t capacity);

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
