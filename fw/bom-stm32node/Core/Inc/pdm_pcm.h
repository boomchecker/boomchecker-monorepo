/**
 ******************************************************************************
 * @file    pdm_pcm.h
 * @brief   PDM -> PCM DSP (CIC5 decimation + DC blocker + FIR low-pass).
 *
 * Pure DSP, no HAL dependency. Ported and encapsulated from the proven Mik_stm
 * project (Core/Src/main.c). All state lives in pdm_pcm_t, so the module is
 * reusable and holds no global state.
 *
 * Chain: 16-bit PDM stream from SAI -> CIC 5th order (D=64, slot selected by a
 *        mask) -> DC blocker (tau ~43 ms) -> FIR 8 kHz @ 48 kHz (q15) ->
 *        gain + saturation.
 ******************************************************************************
 */
#ifndef PDM_PCM_H
#define PDM_PCM_H

#include <stdint.h>
#include <stddef.h>

/* --- Acquisition parameters (must match the SAI/DMA config in CubeMX) ------ */
#define PDM_SCK_HZ           6144000u                 /* SAI bit clock (SCK)      */
#define PDM_RING_HALFWORDS   16384u                   /* 32 kB circular DMA buffer */

/* GPDMA has a 16-bit block counter (BNDT) - the whole ring in bytes must fit. */
#if (PDM_RING_HALFWORDS * 2u) > 65535u
#error "PDM ring exceeds the maximum GPDMA block size"
#endif

/* Output: CIC5 D=64 (mask selects 8 of the 16 frame bits), 8 halfwords = 1 sample. */
#define PCM_FS_HZ            (PDM_SCK_HZ / 2u / 64u)                 /* 48000        */
#define PCM_SAMPLES_PER_HALF (PDM_RING_HALFWORDS * 16u / 2u / 128u) /* 1024         */
#define PCM_GAIN             16                                     /* +24 dB       */
#define FIR_TAPS             101u

/* Slot mask: selects the bits of one microphone from the 16-bit frame.
   0xF807 = channel A (lowest noise in Mik_stm, -92 dBFS), 0x07F8 = channel B. */
#define PDM_SLOT_MASK_A      0xF807u
#define PDM_SLOT_MASK_B      0x07F8u

/**
 * @brief DSP chain state (continuous across circular-buffer half boundaries).
 */
typedef struct
{
  uint64_t cic_i1, cic_i2, cic_i3, cic_i4, cic_i5; /* CIC integrators */
  uint64_t cic_c1, cic_c2, cic_c3, cic_c4, cic_c5; /* CIC combs       */
  int32_t  dc_acc;                                 /* DC blocker accumulator (Q11) */
  uint8_t  dc_seeded;                              /* seeding counter (0..8)       */
  uint32_t pcm_mute;                               /* mute ramp after reset        */
  uint16_t slot_mask;                              /* microphone slot selection    */
  int16_t  fir_x[FIR_TAPS - 1u + PCM_SAMPLES_PER_HALF]; /* FIR history + block     */
} pdm_pcm_t;

/**
 * @brief Initialize/reset the DSP state.
 * @param st        DSP state
 * @param slot_mask slot selection mask (e.g. PDM_SLOT_MASK_A)
 */
void pdm_pcm_init(pdm_pcm_t *st, uint16_t slot_mask);

/**
 * @brief Process one half of the circular buffer into a PCM block.
 * @param st  DSP state
 * @param src PDM data, PDM_RING_HALFWORDS/2 (= 8192) halfwords
 * @param dst output PCM, PCM_SAMPLES_PER_HALF (= 1024) int16 samples
 */
void pdm_pcm_process_half(pdm_pcm_t *st, const uint16_t *src, int16_t *dst);

#endif /* PDM_PCM_H */
