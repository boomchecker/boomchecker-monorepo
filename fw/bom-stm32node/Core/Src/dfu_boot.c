/**
 ******************************************************************************
 * @file    dfu_boot.c
 * @brief   Jump into the STM32H5 ROM bootloader for USB DFU flashing.
 ******************************************************************************
 */
#include "dfu_boot.h"
#include "main.h"

/* System memory base for STM32H562/H563/H573 per AN2606 (bootloader vector
   table). Beware: 0x0BF87000 is the STM32H503 address - jumping there on an
   H563 hard-faults until reset (observed 2026-08-07). */
#define DFU_SYSMEM_BASE 0x0BF97000UL

extern PCD_HandleTypeDef hpcd_USB_DRD_FS; /* CubeMX usb.c */

void dfu_boot_enter(void)
{
  /* Drop off the bus first so the host sees a clean bootloader enumeration. */
  (void)HAL_PCD_Stop(&hpcd_USB_DRD_FS);
  HAL_Delay(50);

  __disable_irq();
  HAL_RCC_DeInit();

  SysTick->CTRL = 0u;
  SysTick->LOAD = 0u;
  SysTick->VAL  = 0u;

  /* The application enables ICACHE in main(); the bootloader expects it off. */
  ICACHE->CR &= ~ICACHE_CR_EN;

  for (uint32_t i = 0u; i < 8u; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFFu;
    NVIC->ICPR[i] = 0xFFFFFFFFu;
  }
  __enable_irq();

  const uint32_t *vectors = (const uint32_t *)DFU_SYSMEM_BASE;
  __set_MSPLIM(0u); /* M33: stale stack limit would fault the bootloader */
  __set_MSP(vectors[0]);
  ((void (*)(void))vectors[1])();

  while (1)
  {
  }
}
