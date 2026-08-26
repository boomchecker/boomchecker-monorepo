/**
 ******************************************************************************
 * @file    boomlink_flash_storage_port.c
 ******************************************************************************
 */
#include "boomlink_flash_storage_port.h"

#include <string.h>

#include "main.h" /* stm32h5xx_hal.h and the FLASH_* HAL API (HAL_FLASH_MODULE_ENABLED) */

/* The reserved region STM32H563xx_FLASH.ld's FLASH region no longer covers -
   see that file's own comment. Bank 2 (not Bank 1) because this is the LAST
   16K of the device's 2MB, and this part's two banks are contiguous 1MB
   halves (Bank 1: 0x08000000-0x080FFFFF, Bank 2: 0x08100000-0x081FFFFF) -
   the last bytes of the whole device are therefore in Bank 2, not Bank 1.
   This part's real sector geometry (Drivers/CMSIS/Device/ST/STM32H5xx/
   Include/stm32h563xx.h: FLASH_SECTOR_NB=128, FLASH_SECTOR_SIZE=0x2000, i.e.
   128 SECTORS OF 8K PER BANK, not the 64-sectors-of-16K an earlier version
   of this file wrongly assumed - a mistake that erased the wrong 8K of live
   firmware on every save() and never erased the actual storage region at
   all, since it happened to land on the right BASE ADDRESS by coincidence
   (63*16K == 126*8K == 0x7E000) while erasing only one 8K sector (63) of
   what needs two. The last 16K is sectors 126 and 127 (0-indexed):
   0x08100000 + 126*8K = 0x081FC000, + 127*8K = 0x081FE000, ending exactly at
   0x08200000. */
#define FLASH_STORAGE_BASE_ADDRESS 0x081FC000u
#define FLASH_STORAGE_SIZE         0x4000u /* 16K = two 8K sectors */
#define FLASH_STORAGE_BANK         FLASH_BANK_2
#define FLASH_STORAGE_FIRST_SECTOR 126u
#define FLASH_STORAGE_SECTOR_COUNT 2u

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
      .Sector    = FLASH_STORAGE_FIRST_SECTOR,
      .NbSectors = FLASH_STORAGE_SECTOR_COUNT,
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
     exactly that before ever calling write(). That granularity/alignment
     half of the contract is trusted, not re-checked, the same way
     radio_port.c trusts radio.h's own contracts - but UNLIKE that case, an
     out-of-range offset/len here would not just be a caller bug this port
     "doesn't need to survive": it would silently HAL_FLASH_Program() into
     whatever firmware flash happens to sit past this region, not merely
     misbehave inside memory this port owns. Cheap enough to check outright
     rather than accept that blast radius on a contract violation. */
  if ((size_t)offset + len > FLASH_STORAGE_SIZE) {
    return false;
  }

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
  /* Same offset/len bound as flash_write() and for the same reason: an
     out-of-range read here is a plain memcpy() from wherever that lands in
     the address space, not a bounds-checked call into a fake's own buffer
     the way tests/config_store_test.c's fake_read() is. */
  if ((size_t)offset + len > FLASH_STORAGE_SIZE) {
    return false;
  }
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
