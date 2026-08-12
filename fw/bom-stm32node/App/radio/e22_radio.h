/**
 ******************************************************************************
 * @file    e22_radio.h
 * @brief   EBYTE E22-900M22S (SX1262) board bring-up: power sequencing and
 *          module-specific RadioLib configuration.
 *
 * Internal to the radio layer - included only by radio.cpp, never by
 * application code (see radio.h for the public C API).
 ******************************************************************************
 */
#ifndef E22_RADIO_H
#define E22_RADIO_H

#include <RadioLib.h>

namespace e22_radio {

/* The bring-up LoRa PHY profile. See
   docs/firmware/bom-stm32node/radio-profile.md for the regulatory rationale
   (869.4-869.65 MHz sub-band, ERP/duty-cycle limits) and why these values
   were picked. */
struct Profile {
  float    frequencyMhz;
  float    bandwidthKhz;
  uint8_t  spreadingFactor;
  uint8_t  codingRateDenom;
  uint8_t  syncWord;
  int8_t   txPowerDbm;
  uint16_t preambleSymbols;
  float    tcxoVoltage;
};

/* Compliant-by-default bring-up profile (869.525 MHz, BW125/SF7/CR4:5,
   14 dBm, private sync word). Not a final field profile - see
   docs/firmware/bom-stm32node/radio-profile.md. */
const Profile &DefaultProfile();

/* Power up the E22 module (EN_LORA) and wait for its regulator/TCXO to
   settle. NRST is already held low from boot (gpio.c); the actual reset
   pulse happens inside SX1262::begin() via Stm32RadioLibHal. Call once,
   before constructing/using the SX1262 object. */
void PowerUp();

/* Apply E22-specific RadioLib configuration that begin() does not cover
   (RXEN/TXEN RF-switch pins). Call once, after constructing the SX1262
   object and before begin(). */
void ConfigureModule(SX1262 &radio);

} // namespace e22_radio

#endif /* E22_RADIO_H */
