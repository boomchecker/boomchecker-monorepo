/**
 ******************************************************************************
 * @file    mic.h
 * @brief   PDM microphone acquisition: SAI1_A + GPDMA circular -> PCM.
 *
 * Sits on top of SAI1, which CubeMX configures (PDM master RX, 16-bit frame,
 * 1 slot, CK1 = 3.072 MHz). This module owns the circular DMA buffer, builds
 * the GPDMA1_Channel0 circular linked-list, starts/stops reception and
 * converts completed halves into 48 kHz PCM via pdm_pcm.
 *
 * Assumptions (see docs/pdm-port-plan.md, "CubeMX contract"):
 *   - hsai_BlockA1: SAI1_A PDM master RX, DataSize 16, 1 slot, D1 = PE6
 *   - GPDMA1 enabled, GPDMA1_Channel0 free (the linked-list is built here)
 ******************************************************************************
 */
#ifndef MIC_H
#define MIC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "pdm_pcm.h"

/**
 * @brief Build the GPDMA1_Channel0 circular linked-list and link it to SAI1_A.
 *        Call once after MX_SAI1_Init (in USER CODE 2 in main.c).
 */
void mic_dma_init(void);

/**
 * @brief Reset the DSP state and start continuous DMA reception into the ring.
 * @return HAL_OK on success
 */
int mic_start(void);

/**
 * @brief Stop DMA reception.
 */
void mic_stop(void);

/**
 * @brief Select which microphone of the PDM pair the DSP demodulates
 *        (PDM_SLOT_MASK_A / PDM_SLOT_MASK_B). Takes effect on the next
 *        mic_start(); which one is populated is a board property, so the
 *        firmware default is only a default. See `micslot` in cli.c.
 */
void mic_set_slot_mask(uint16_t mask);

/** @brief The slot mask the next mic_start() will use. */
uint16_t mic_get_slot_mask(void);

/**
 * @brief Wiring diagnostics: sample PDM_D1/PDM_D2 as GPIO inputs with the PDM
 *        clock running (toggle counts) and pull-test them with the clock off.
 *        Prints MICDIAG lines and a MICDIAGEND trailer on the console.
 */
void mic_diag_run(void);

/**
 * @brief Process one completed half of the ring (if one is ready).
 * @param pcm   output buffer, at least PCM_SAMPLES_PER_HALF int16 samples
 * @param nsamp [out] number of samples written (PCM_SAMPLES_PER_HALF)
 * @return true if a half was processed, false otherwise (nothing ready)
 */
bool mic_poll(int16_t *pcm, size_t *nsamp);

/**
 * @brief true if an overrun happened since start (main did not keep up).
 */
bool mic_overrun(void);

/**
 * @brief Number of halves processed so far (to check the DMA is running).
 */
uint32_t mic_blocks_processed(void);

#endif /* MIC_H */
