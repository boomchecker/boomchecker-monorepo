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

#include <cstring>

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
  /* EN_LORA/NRST transitions during power-up can latch a stale edge on this
     line before the NVIC is armed; clear it first so the first
     radio_process() call reacts to a real DIO1 event, not a power-up
     artifact. */
  __HAL_GPIO_EXTI_CLEAR_RISING_IT(LORA_DIO1_Pin);
  HAL_NVIC_ClearPendingIRQ(EXTI2_IRQn);
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
  /* DWT->CYCCNT is a free-running 32-bit HARDWARE counter, so taking deltas
     directly on it wraps safely at the full 2^32 range. Naively returning
     CYCCNT/250 is NOT equivalently safe: dividing shrinks the wrap period of
     the returned VALUE to ~17.18 s (2^32 / 250 us) while its type can still
     hold values up to ~4.29e9 - so two micros() calls straddling that
     ~17.18 s wrap would subtract to a huge bogus delta instead of the true
     (small) one. RADIOLIB_SPI_PARANOID (on by default) times SPI register
     writes against micros() on every begin()/config call, so this is not
     just a theoretical risk. Accumulate in cycle space instead, where
     wraparound subtraction is actually valid, and only convert to
     microseconds - and truncate to the 32-bit RadioLibTime_t - at the end;
     the truncated value then wraps correctly like any ordinary counter. */
  static uint32_t lastCycles  = DWT->CYCCNT;
  static uint64_t microsAccum = 0;

  uint32_t cycles      = DWT->CYCCNT;
  uint32_t deltaCycles = cycles - lastCycles;
  lastCycles = cycles;
  microsAccum += deltaCycles / (SystemCoreClock / 1000000UL);
  return static_cast<RadioLibTime_t>(microsAccum);
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
  HAL_StatusTypeDef status =
      HAL_SPI_TransmitReceive(&hspi1, out, in, static_cast<uint16_t>(len), kSpiTransferTimeoutMs);
  if (status != HAL_OK && in != nullptr && len > 0) {
    /* RadioLibHal::spiTransfer() returns void - there is no channel to
       report a transport failure back to RadioLib itself. Zero the receive
       buffer instead of leaving whatever was on the stack before this call:
       RadioLib parses `in` as the chip's status/response bytes, and an
       untouched buffer could be misread as a valid (if wrong) reply rather
       than the communication failure it actually is. */
    memset(in, 0, len);
  }
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

/* This STM32H5 HAL splits the generic EXTI callback other families share into
   separate rising/falling variants (stm32h5xx_hal_gpio.c); LORA_DIO1 is
   configured GPIO_MODE_IT_RISING (gpio.c), so only the rising edge is ever
   relevant here. Guard on the pin anyway: LORA_DIO2/IMU_INTn/GPS_1PPS are
   also configured GPIO_MODE_IT_RISING in gpio.c (pre-existing, unrelated to
   the radio), just not yet enabled in the NVIC. */
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == LORA_DIO1_Pin) {
    stm32_radiolib_hal_instance().handleDio1Isr();
  }
}

} // extern "C"
