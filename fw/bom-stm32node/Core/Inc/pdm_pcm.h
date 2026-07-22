/**
 ******************************************************************************
 * @file    pdm_pcm.h
 * @brief   PDM -> PCM DSP (CIC5 decimace + DC blocker + FIR dolni propust).
 *
 * Cisty DSP, bez zavislosti na HAL. Prevzato a zapouzdreno z odladeneho
 * projektu Mik_stm (Core/Src/main.c). Vsechen stav je v pdm_pcm_t, takze
 * modul je znovupouzitelny a nema globalni stav.
 *
 * Retezec: 16bit PDM stream ze SAI -> CIC 5. radu (D=64, vyber slotu maskou)
 *          -> DC blocker (tau ~43 ms) -> FIR 8 kHz @ 48 kHz (q15) -> gain+saturace.
 ******************************************************************************
 */
#ifndef PDM_PCM_H
#define PDM_PCM_H

#include <stdint.h>
#include <stddef.h>

/* --- Parametry akvizice (musi sedet s SAI/DMA konfiguraci v CubeMX) -------- */
#define PDM_SCK_HZ           6144000u                 /* SAI bitclock (SCK)       */
#define PDM_RING_HALFWORDS   16384u                   /* 32 kB kruhovy DMA buffer */

/* GPDMA ma 16bitovy citac bloku (BNDT) - cely ring v bajtech se do nej musi vejit */
#if (PDM_RING_HALFWORDS * 2u) > 65535u
#error "PDM ring presahuje maximalni velikost GPDMA bloku"
#endif

/* Vystup: CIC5 D=64 (maska vybira 8 z 16 bitu ramce), 8 halfwordu = 1 vzorek. */
#define PCM_FS_HZ            (PDM_SCK_HZ / 2u / 64u)                 /* 48000        */
#define PCM_SAMPLES_PER_HALF (PDM_RING_HALFWORDS * 16u / 2u / 128u) /* 1024         */
#define PCM_GAIN             16                                     /* +24 dB       */
#define FIR_TAPS             101u

/* Maska slotu: vybira bity jednoho mikrofonu z 16bit ramce. 0xF807 = kanal A
   (v Mik_stm dal nejnizsi sum, -92 dBFS), 0x07F8 = kanal B. */
#define PDM_SLOT_MASK_A      0xF807u
#define PDM_SLOT_MASK_B      0x07F8u

/**
 * @brief Stav DSP retezce (spojity pres hranice polovin kruhoveho bufferu).
 */
typedef struct
{
  uint64_t cic_i1, cic_i2, cic_i3, cic_i4, cic_i5; /* CIC integratory */
  uint64_t cic_c1, cic_c2, cic_c3, cic_c4, cic_c5; /* CIC comby       */
  int32_t  dc_acc;                                 /* DC blocker akumulator (Q11) */
  uint8_t  dc_seeded;                              /* pocitadlo seedovani (0..8)  */
  uint32_t pcm_mute;                               /* utlum nabehu filtru po resetu */
  uint16_t slot_mask;                              /* vyber slotu mikrofonu       */
  int16_t  fir_x[FIR_TAPS - 1u + PCM_SAMPLES_PER_HALF]; /* FIR historie + blok    */
} pdm_pcm_t;

/**
 * @brief Inicializuje/resetuje DSP stav.
 * @param st        stav DSP
 * @param slot_mask maska vyberu slotu (napr. PDM_SLOT_MASK_A)
 */
void pdm_pcm_init(pdm_pcm_t *st, uint16_t slot_mask);

/**
 * @brief Zpracuje polovinu kruhoveho bufferu na blok PCM.
 * @param st  stav DSP
 * @param src PDM data, PDM_RING_HALFWORDS/2 (= 8192) halfwordu
 * @param dst vystupni PCM, PCM_SAMPLES_PER_HALF (= 1024) vzorku int16
 */
void pdm_pcm_process_half(pdm_pcm_t *st, const uint16_t *src, int16_t *dst);

#endif /* PDM_PCM_H */
