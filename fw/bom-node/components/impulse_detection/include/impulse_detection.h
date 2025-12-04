#ifndef IMPULSE_DETECTION_H
#define IMPULSE_DETECTION_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Struktura pro konfigurační parametry detekce.
 * Hodnoty odpovídají popisu v diplomce (Opocenský, 2021)
 */
typedef struct {
  int num_taps;       // počet tapů (NumTaps)
  int tap_size;       // délka jednoho tapu (TapSize)
  float det_rms;      // násobek RMS prahu (DetRMS)
  uint32_t det_level; // minimální hodnota (DetLevel)
  float det_energy;   // poměr energií B/A (DetEnergy)
} impulse_detection_params_t;

/**
 * Inicializace detektoru s vlastní konfigurací
 * (pokud necháš NULL, použije se výchozí)
 */
void impulse_detection_init(const impulse_detection_params_t *params);

/**
 * Hlavní funkce pro detekci impulsu.
 * samples - pole int16_t vzorků (např. z jednoho mikrofonu)
 * num_samples - délka pole
 *
 * Vrací true, pokud byl detekován impuls.
 */
bool impulse_detect(const int16_t *samples, int num_samples);

#endif // IMPULSE_DETECTION_H
