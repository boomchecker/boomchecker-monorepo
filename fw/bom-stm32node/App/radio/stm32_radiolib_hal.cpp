/**
 ******************************************************************************
 * @file    stm32_radiolib_hal.cpp
 * @brief   RadioLib platform HAL backed by STM32 HAL (SPI1 + GPIO + EXTI).
 ******************************************************************************
 */
#include "stm32_radiolib_hal.hpp"

extern "C" {
#include "main.h" /* LORA_*_Pin / LORA_*_GPIO_Port, Error_Handler */
#include "spi.h"  /* hspi1 */
}

namespace {

/* GpioLevelLow/High values Stm32RadioLibHal reports to RadioLib through the
   base-class constructor; digitalWrite()/digitalRead() translate them to/from
   GPIO_PIN_RESET/SET. GpioModeInput/Output and GpioInterruptRising/Falling are
   accepted from RadioLib but otherwise unused - see pinMode()/attachInterrupt(). */
constexpr uint32_t kGpioModeInput        = 0;
constexpr uint32_t kGpioModeOutput       = 1;
constexpr uint32_t kGpioLevelLow         = 0;
constexpr uint32_t kGpioLevelHigh        = 1;
constexpr uint32_t kGpioInterruptRising  = 1;
constexpr uint32_t kGpioInterruptFalling = 2;

/* DIO1 (LORA_DIO1/PB2) is wired to EXTI line 2. Priority must stay numerically
   higher (less urgent) than the audio GPDMA1_Channel0_IRQn (priority 5, see
   mic.c) so radio activity can never delay/overrun the microphone DMA - see
   docs/firmware/bom-stm32node/boomlink.md section 6. */
constexpr uint32_t kDio1IrqPreemptPriority = 6;

constexpr uint32_t kSpiTransferTimeoutMs = 100;

bool ResolvePin(uint32_t pin, GPIO_TypeDef **port, uint16_t *mask) {
  switch (static_cast<Stm32RadioPin>(pin)) {
    case RADIO_PIN_NSS:  *port = LORA_NSS_GPIO_Port;  *mask = LORA_NSS_Pin;  return true;
    case RADIO_PIN_DIO1: *port = LORA_DIO1_GPIO_Port; *mask = LORA_DIO1_Pin; return true;
    case RADIO_PIN_NRST: *port = LORA_NRST_GPIO_Port; *mask = LORA_NRST_Pin; return true;
    case RADIO_PIN_BUSY: *port = LORA_BUSY_GPIO_Port; *mask = LORA_BUSY_Pin; return true;
    case RADIO_PIN_RXEN: *port = LORA_RXEN_GPIO_Port; *mask = LORA_RXEN_Pin; return true;
    case RADIO_PIN_TXEN: *port = LORA_TXEN_GPIO_Port; *mask = LORA_TXEN_Pin; return true;
    default:             return false; /* e.g. RADIOLIB_NC */
  }
}

} // namespace

Stm32RadioLibHal &stm32_radiolib_hal_instance() {
  static Stm32RadioLibHal hal;
  return hal;
}

Stm32RadioLibHal::Stm32RadioLibHal()
  : RadioLibHal(kGpioModeInput, kGpioModeOutput, kGpioLevelLow, kGpioLevelHigh,
                kGpioInterruptRising, kGpioInterruptFalling),
    m_dio1Callback(nullptr) {
  /* Enable the Cortex-M33 DWT cycle counter for micros()/delayMicroseconds().
     Idempotent - safe even if something else already enabled it. */
  DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void Stm32RadioLibHal::pinMode(uint32_t pin, uint32_t mode) {
  /* Every radio GPIO - including LORA_DIO1's rising-edge EXTI trigger - is
     already configured once by CubeMX (gpio.c) and the SPI1 fix (spi.c)
     before radio_init() ever runs. Module::init()/SX126x::reset()/modSetup()
     call pinMode(cs/rst, OUTPUT) and pinMode(irq/gpio, INPUT) unconditionally;
     re-running a generic HAL_GPIO_Init here for LORA_DIO1 would silently
     strip its EXTI trigger configuration, so this is intentionally a no-op:
     RadioLib only ever asks for modes the static board configuration already
     guarantees. */
  (void)pin;
  (void)mode;
}

void Stm32RadioLibHal::digitalWrite(uint32_t pin, uint32_t value) {
  GPIO_TypeDef *port;
  uint16_t mask;
  if (!ResolvePin(pin, &port, &mask)) {
    return;
  }
  HAL_GPIO_WritePin(port, mask, (value == GpioLevelHigh) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint32_t Stm32RadioLibHal::digitalRead(uint32_t pin) {
  GPIO_TypeDef *port;
  uint16_t mask;
  if (!ResolvePin(pin, &port, &mask)) {
    return GpioLevelLow;
  }
  return (HAL_GPIO_ReadPin(port, mask) == GPIO_PIN_SET) ? GpioLevelHigh : GpioLevelLow;
}

void Stm32RadioLibHal::attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) {
  (void)mode; /* edge is fixed at GPIO_MODE_IT_RISING in gpio.c; SX126x always requests rising */
  if (interruptNum != RADIO_PIN_DIO1) {
    return;
  }
  m_dio1Callback = interruptCb;
  HAL_NVIC_SetPriority(EXTI2_IRQn, kDio1IrqPreemptPriority, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);
}

void Stm32RadioLibHal::detachInterrupt(uint32_t interruptNum) {
  if (interruptNum != RADIO_PIN_DIO1) {
    return;
  }
  HAL_NVIC_DisableIRQ(EXTI2_IRQn);
  m_dio1Callback = nullptr;
}

void Stm32RadioLibHal::delay(RadioLibTime_t ms) {
  HAL_Delay(static_cast<uint32_t>(ms));
}

void Stm32RadioLibHal::delayMicroseconds(RadioLibTime_t us) {
  uint32_t cycles = static_cast<uint32_t>(us) * (SystemCoreClock / 1000000UL);
  uint32_t start = DWT->CYCCNT;
  while ((DWT->CYCCNT - start) < cycles) {
    /* busy-wait */
  }
}

RadioLibTime_t Stm32RadioLibHal::millis() {
  return static_cast<RadioLibTime_t>(HAL_GetTick());
}

RadioLibTime_t Stm32RadioLibHal::micros() {
  /* DWT->CYCCNT is a free-running 32-bit cycle counter; at 250 MHz this wraps
     (and so does the microsecond value derived from it) roughly every 17 s.
     RadioLib only ever compares micros() deltas against timeouts of at most a
     few seconds, so unsigned-wraparound subtraction keeps this safe. */
  return static_cast<RadioLibTime_t>(DWT->CYCCNT / (SystemCoreClock / 1000000UL));
}

long Stm32RadioLibHal::pulseIn(uint32_t pin, uint32_t state, RadioLibTime_t timeout) {
  /* Not used by the SX126x driver path (BUSY is polled directly via
     digitalRead), but is a pure-virtual RadioLibHal method and must be
     implemented for a generic edge-to-edge pulse measurement. */
  GPIO_TypeDef *port;
  uint16_t mask;
  if (!ResolvePin(pin, &port, &mask)) {
    return 0;
  }
  GPIO_PinState wantLevel = (state == GpioLevelHigh) ? GPIO_PIN_SET : GPIO_PIN_RESET;

  RadioLibTime_t start = micros();
  while (HAL_GPIO_ReadPin(port, mask) != wantLevel) {
    if ((micros() - start) > timeout) {
      return 0;
    }
  }
  RadioLibTime_t pulseStart = micros();
  while (HAL_GPIO_ReadPin(port, mask) == wantLevel) {
    if ((micros() - pulseStart) > timeout) {
      return 0;
    }
  }
  return static_cast<long>(micros() - pulseStart);
}

void Stm32RadioLibHal::spiBegin() {
  /* SPI1 is dedicated to the radio and already configured by MX_SPI1_Init()
     (8-bit, software NSS, <=16 MHz) before radio_init() runs. */
}

void Stm32RadioLibHal::spiBeginTransaction() {
  /* Nothing to arbitrate: SPI1 has exactly one device on its bus (the E22
     module). NSS itself is toggled by RadioLib via digitalWrite(csPin, ...)
     around spiTransfer(), not here. */
}

void Stm32RadioLibHal::spiTransfer(uint8_t *out, size_t len, uint8_t *in) {
  HAL_SPI_TransmitReceive(&hspi1, out, in, static_cast<uint16_t>(len), kSpiTransferTimeoutMs);
}

void Stm32RadioLibHal::spiEndTransaction() {
  /* See spiBeginTransaction(). */
}

void Stm32RadioLibHal::spiEnd() {
  /* SPI1 stays initialized for the lifetime of the firmware. */
}

void Stm32RadioLibHal::handleDio1Isr() {
  if (m_dio1Callback != nullptr) {
    m_dio1Callback();
  }
}

extern "C" {

/* DIO1 EXTI. Previously configured (GPIO_MODE_IT_RISING, gpio.c) but never
   enabled in the NVIC or handled - see
   docs/firmware/bom-stm32node/boomlink.md section 6 and PR1 scope. Mirrors
   mic.c's GPDMA1_Channel0_IRQHandler: the radio module owns this handler
   directly instead of routing it through the CubeMX-generated
   stm32h5xx_it.c/.h, since NVIC enablement/priority here is also owned by
   attachInterrupt() above rather than by CubeMX. */
void EXTI2_IRQHandler(void) {
  HAL_GPIO_EXTI_IRQHandler(LORA_DIO1_Pin);
}

/* HAL's generic per-pin EXTI callback. Only LORA_DIO1 is enabled in the NVIC
   today, but guard on the pin anyway in case another EXTI-configured pin
   (LORA_DIO2, IMU_INTn, GPS_1PPS) is ever enabled later. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == LORA_DIO1_Pin) {
    stm32_radiolib_hal_instance().handleDio1Isr();
  }
}

} // extern "C"
