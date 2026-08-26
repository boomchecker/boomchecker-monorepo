/**
 ******************************************************************************
 * @file    boomlink_flash_storage_port.c
 ******************************************************************************
 */
#include "boomlink_flash_storage_port.h"

#include <string.h>

#include "main.h" /* stm32h5xx_hal.h and the FLASH_* HAL API (HAL_FLASH_MODULE_ENABLED) */

/* The reserved sector STM32H563xx_FLASH.ld's FLASH region no longer covers -
   see that file's own comment. Bank 2 (not Bank 1) because this is the LAST
   sector of the device's 2MB, and this part's two banks are contiguous
   1MB halves (Bank 1: 0x08000000-0x080FFFFF, Bank 2: 0x08100000-
   0x081FFFFF) - the last sector of the whole device is therefore the last
   sector of Bank 2, not Bank 1. Sector 63 of 64 (16K sectors, 0-indexed) is
   that bank's last sector: 0x08100000 + 63*16K = 0x081FC000. */
#define FLASH_STORAGE_BASE_ADDRESS 0x081FC000u
#define FLASH_STORAGE_SIZE         0x4000u /* 16K, one sector */
#define FLASH_STORAGE_BANK         FLASH_BANK_2
#define FLASH_STORAGE_SECTOR       63u

/* HAL_FLASH_Program's own granularity on this part (see
   FLASH_TYPEPROGRAM_QUADWORD's doc, stm32h5xx_hal_flash.h) - one program
   call writes exactly 16 bytes, aligned. Not TrustZone-suffixed (_NS/_S):
   this board has TrustZone disabled (HAL_GTZC_MODULE_ENABLED is not defined,
   Core/Inc/stm32h5xx_hal_conf.h), so the plain HAL_FLASH_* calls already
   operate on the whole (non-partitioned) flash. */
#define FLASH_STORAGE_WRITE_GRANULARITY 16u

static bool flash_erase(void *ctx) {
  (void)ctx;

  if (HAL_FLASH_Unlock() != HAL_OK) {
    return false;
  }

  FLASH_EraseInitTypeDef erase_init = {
      .TypeErase = FLASH_TYPEERASE_SECTORS,
      .Banks     = FLASH_STORAGE_BANK,
      .Sector    = FLASH_STORAGE_SECTOR,
      .NbSectors = 1u,
  };
  uint32_t          sector_error = 0xFFFFFFFFu;
  HAL_StatusTypeDef status       = HAL_FLASHEx_Erase(&erase_init, &sector_error);

  HAL_FLASH_Lock();
  return status == HAL_OK;
}

static bool flash_write(void *ctx, uint32_t offset, const uint8_t *data, size_t len) {
  (void)ctx;

  /* boomlink_storage_port_t's own contract (boomlink_storage_port.h) is that
     `len` is always a write_granularity multiple and `offset` is always
     aligned to it - boomlink_config_store_save() (the only caller) pads to
     exactly that before ever calling write(). Trusted here rather than
     re-checked, the same way radio_port.c trusts radio.h's own contracts:
     an out-of-contract call is this file's caller's bug, not a hardware
     condition this port needs to survive. */
  if (HAL_FLASH_Unlock() != HAL_OK) {
    return false;
  }

  bool ok = true;
  for (size_t written = 0u; written < len; written += FLASH_STORAGE_WRITE_GRANULARITY) {
    uint32_t dest = FLASH_STORAGE_BASE_ADDRESS + offset + (uint32_t)written;
    uint32_t src  = (uint32_t)(uintptr_t)&data[written];
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, dest, src) != HAL_OK) {
      ok = false;
      break;
    }
  }

  HAL_FLASH_Lock();
  return ok;
}

static bool flash_read(void *ctx, uint32_t offset, uint8_t *out, size_t len) {
  (void)ctx;
  /* Flash is memory-mapped and directly readable on this part - no HAL call
     needed, the same way a real device's read() on any memory-mapped medium
     would be plain memory access rather than a peripheral transaction. */
  memcpy(out, (const void *)(uintptr_t)(FLASH_STORAGE_BASE_ADDRESS + offset), len);
  return true;
}

void boomlink_flash_storage_port_init(boomlink_storage_port_t *out) {
  if (out == NULL) {
    return;
  }

  out->erase             = flash_erase;
  out->write             = flash_write;
  out->read              = flash_read;
  out->region_size       = FLASH_STORAGE_SIZE;
  out->write_granularity = FLASH_STORAGE_WRITE_GRANULARITY;
  out->ctx               = NULL;
}
