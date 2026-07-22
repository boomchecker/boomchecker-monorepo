/**
 ******************************************************************************
 * @file    mic.h
 * @brief   Akvizice z PDM mikrofonu: SAI1_A + GPDMA circular -> PCM.
 *
 * Nadstavba nad SAI1, ktere konfiguruje CubeMX (PDM master RX, 16bit ramec,
 * 1 slot, CK1 = 3.072 MHz). Tento modul si vlastni kruhovy DMA buffer,
 * postavi GPDMA1_Channel0 circular linked-list, spousti/zastavuje prijem a
 * prevadi hotove poloviny na 48 kHz PCM pres pdm_pcm.
 *
 * Predpoklady (viz docs/pdm-port-plan.md, "CubeMX kontrakt"):
 *   - hsai_BlockA1: SAI1_A PDM master RX, DataSize 16, 1 slot, D1 = PE6
 *   - GPDMA1 povolene, GPDMA1_Channel0 volny (linked-list stavime zde)
 ******************************************************************************
 */
#ifndef MIC_H
#define MIC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "pdm_pcm.h"

/**
 * @brief Postavi GPDMA1_Channel0 circular linked-list a nalinkuje ho na SAI1_A.
 *        Volat jednou po MX_SAI1_Init (v USER CODE 2 v main.c).
 */
void mic_dma_init(void);

/**
 * @brief Resetuje DSP stav a spusti kontinualni DMA prijem do kruhoveho bufferu.
 * @return HAL_OK pri uspechu
 */
int mic_start(void);

/**
 * @brief Zastavi DMA prijem.
 */
void mic_stop(void);

/**
 * @brief Zpracuje jednu hotovou polovinu kruhoveho bufferu (pokud je pripravena).
 * @param pcm   vystupni buffer, min. PCM_SAMPLES_PER_HALF vzorku int16
 * @param nsamp [out] pocet zapsanych vzorku (PCM_SAMPLES_PER_HALF)
 * @return true pokud byla polovina zpracovana, jinak false (nic hotoveho)
 */
bool mic_poll(int16_t *pcm, size_t *nsamp);

/**
 * @brief true pokud od startu doslo k overrunu (main nestihal zpracovat polovinu).
 */
bool mic_overrun(void);

/**
 * @brief Pocet dosud zpracovanych polovin (pro diagnostiku behu DMA).
 */
uint32_t mic_blocks_processed(void);

#endif /* MIC_H */
