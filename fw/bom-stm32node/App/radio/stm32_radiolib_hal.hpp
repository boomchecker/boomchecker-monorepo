/**
 ******************************************************************************
 * @file    stm32_radiolib_hal.hpp
 * @brief   RadioLib platform HAL backed by STM32 HAL (SPI1 + GPIO + EXTI).
 *
 * Pure platform glue: SPI transfers, GPIO read/write, timing and interrupt
 * attach/detach for RadioLib. Knows nothing about SX1262/E22 specifics (that
 * lives in e22_radio.cpp) or BoomProtocol/BoomLink.
 ******************************************************************************
 */
#ifndef STM32_RADIOLIB_HAL_HPP
#define STM32_RADIOLIB_HAL_HPP

#include <Hal.h>

/* Logical pin identifiers passed through RadioLib's Module/Hal API (which
   treats "pin" as an opaque platform uint32_t). Stm32RadioLibHal maps each
   one to the actual GPIO port/pin defined by CubeMX in Core/Inc/main.h. */
enum Stm32RadioPin : uint32_t {
  RADIO_PIN_NSS = 0,
  RADIO_PIN_DIO1,
  RADIO_PIN_NRST,
  RADIO_PIN_BUSY,
  RADIO_PIN_RXEN,
  RADIO_PIN_TXEN,
};

class Stm32RadioLibHal final : public RadioLibHal {
public:
  Stm32RadioLibHal();

  void pinMode(uint32_t pin, uint32_t mode) override;
  void digitalWrite(uint32_t pin, uint32_t value) override;
  uint32_t digitalRead(uint32_t pin) override;
  void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override;
  void detachInterrupt(uint32_t interruptNum) override;
  void delay(RadioLibTime_t ms) override;
  void delayMicroseconds(RadioLibTime_t us) override;
  RadioLibTime_t millis() override;
  RadioLibTime_t micros() override;
  long pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) override;
  void spiBegin() override;
  void spiBeginTransaction() override;
  void spiTransfer(uint8_t *out, size_t len, uint8_t *in) override;
  void spiEndTransaction() override;
  void spiEnd() override;

  /* Invoked from EXTI2_IRQHandler (LORA_DIO1, PB2) via HAL_GPIO_EXTI_Callback.
     Stays minimal on purpose: it only calls whichever callback RadioLib last
     registered through attachInterrupt(), which per RadioLib's own usage
     pattern is itself just a flag-setter (see radio.cpp). No SPI, no BUSY
     polling, no packet handling ever happens here - that is all deferred to
     radio_process() in the main superloop. */
  void handleDio1Isr();

private:
  void (*m_dio1Callback)(void);
};

/* Process-wide singleton: one physical SX1262 on one SPI1 bus. Shared between
   radio.cpp (constructs the RadioLib Module/SX1262 with it) and the EXTI ISR
   glue defined in stm32_radiolib_hal.cpp. */
Stm32RadioLibHal &stm32_radiolib_hal_instance();

#endif /* STM32_RADIOLIB_HAL_HPP */
