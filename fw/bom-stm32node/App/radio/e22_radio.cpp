/**
 ******************************************************************************
 * @file    e22_radio.cpp
 * @brief   EBYTE E22-900M22S (SX1262) board bring-up.
 ******************************************************************************
 */
#include "e22_radio.h"

#include "stm32_radiolib_hal.hpp"

extern "C" {
#include "main.h" /* EN_LORA_Pin/_GPIO_Port, HAL_Delay */
}

namespace e22_radio {

namespace {

/* EBYTE does not document an exact EN_LORA-to-SPI-ready timing spec beyond
   the SX1262's own NRST pulse width, so this errs generous: enough for the
   module's onboard regulator and 32 MHz TCXO to settle before anything
   touches SPI/NRST (SX1262::begin() drives the actual reset pulse). */
constexpr uint32_t kPowerUpSettleMs = 10;

} // namespace

const Profile &DefaultProfile() {
  /* eByte E22-900M22S/E22-900M30S community configurations consistently use
     a 1.8 V DIO3 TCXO reference (see
     https://github.com/jgromes/RadioLib/discussions/487) - confirm against
     the module's own datasheet/PCB revision before first power-up; a wrong
     value prevents the oscillator from locking and the radio will not
     transmit/receive on frequency at all. */
  static const Profile kProfile = {
    /* frequencyMhz     */ 869.525f,
    /* bandwidthKhz     */ 125.0f,
    /* spreadingFactor  */ 7,
    /* codingRateDenom  */ 5,
    /* syncWord         */ RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
    /* txPowerDbm       */ 14,
    /* preambleSymbols  */ 8,
    /* tcxoVoltage      */ 1.8f,
  };
  return kProfile;
}

void PowerUp() {
  HAL_GPIO_WritePin(EN_LORA_GPIO_Port, EN_LORA_Pin, GPIO_PIN_SET);
  HAL_Delay(kPowerUpSettleMs);
}

void ConfigureModule(SX1262 &radio) {
  /* SX126x automatically drives these on every RX/TX/standby transition
     (Module::setRfSwitchState, called from SX126x::standby/transmit/
     startReceive) - no manual RXEN/TXEN toggling needed elsewhere. */
  radio.setRfSwitchPins(RADIO_PIN_RXEN, RADIO_PIN_TXEN);
}

} // namespace e22_radio
