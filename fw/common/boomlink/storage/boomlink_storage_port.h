/**
 ******************************************************************************
 * @file    boomlink_storage_port.h
 * @brief   The seam between the config storage wrapper (boomlink_config_
 *          store.h) and whatever actually holds the bytes - a real flash
 *          sector on the target, an in-memory buffer in host tests.
 *
 *          boomlink.md section 10.1 describes the wrapper format and boot
 *          behaviour in terms of "internal flash", but nothing about the
 *          wrapper logic itself (framing, CRC, load/save, missing/invalid
 *          fallback) needs a real flash device to test - the same reasoning
 *          that gives the link engine a fake radio (linkengine/
 *          boomlink_port.h) instead of testing only against real hardware.
 *
 *          Deliberately NOT modeled as generic byte-range read/write/erase:
 *          this wrapper owns exactly one fixed-size region and always
 *          erases and rewrites it as a whole (see boomlink_config_store.c's
 *          own doc for why a single region, not a ping-pong pair, is enough
 *          for section 10.1's stated contract), so `erase` and `write` take
 *          no address - there is only ever one thing to erase and one place
 *          to write.
 ******************************************************************************
 */
#ifndef BOOMLINK_STORAGE_PORT_H
#define BOOMLINK_STORAGE_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Everything the config store needs from below it. Every member is
 * required; pass this to boomlink_storage_port_is_valid() before use, the
 * same discipline linkengine/boomlink_port.h's boomlink_port_is_valid()
 * follows and for the same reason - a NULL callback should fail loudly at
 * init, not crash on the first save.
 */
typedef struct {
  /**
   * Erase the entire managed region back to its erased state. A real flash
   * implementation cannot promise this completes atomically against a power
   * loss mid-erase - which is exactly why a torn erase or a torn write both
   * simply look like "invalid" to boomlink_config_store_load() (a bad magic
   * or a bad CRC), not a distinct failure mode this port needs to report.
   *
   * @return true if the region was erased, false on any hardware error.
   */
  bool (*erase)(void *ctx);

  /**
   * Write `len` bytes at `offset` within the managed region. `len` is
   * always a multiple of `write_granularity` and `offset` is always aligned
   * to it - boomlink_config_store.c pads its buffer before calling this, so
   * a real implementation never needs to handle a partial-granule write.
   * The region is always erased (via `erase`) immediately before the first
   * write of a save, never written to twice without an erase between - flash
   * can only clear bits by erasing, not set them back, so writing the same
   * bytes twice without erasing would not reliably produce the second
   * value.
   *
   * @return true if the write succeeded and (where the hardware can tell)
   *         verified, false otherwise.
   */
  bool (*write)(void *ctx, uint32_t offset, const uint8_t *data, size_t len);

  /**
   * Read `len` bytes at `offset` within the managed region into `out`. No
   * alignment requirement - a boot-time load reads the fixed-size header
   * first and only reads the exact `protobuf_length` it declares next, not
   * necessarily a `write_granularity` multiple.
   *
   * @return true if the read succeeded, false otherwise (e.g. out of range).
   */
  bool (*read)(void *ctx, uint32_t offset, uint8_t *out, size_t len);

  /** Total bytes the region holds - the header plus the largest blob it can
   *  ever need to carry, boomlink_NodeConfig_size, must both fit inside it;
   *  boomlink_storage_port_is_valid() checks the first half of that, the
   *  caller-supplied blob size checks the second. */
  size_t region_size;

  /** The only size `write()` may ever be called with is a multiple of this.
   *  16 on the target (STM32H5's quad-word flash program granularity); 1 is
   *  fine for a host test's in-memory fake, which has no such restriction. */
  size_t write_granularity;

  void *ctx;
} boomlink_storage_port_t;

/**
 * Whether `port` is usable, checking only what is generic to any port (this
 * header has no dependency on boomlink_config_store.h - see this file's own
 * doc on the seam being deliberately below the wrapper format, and
 * boomlink_config_store.c's include comment on why that direction must stay
 * one-way): non-NULL, every callback present, a non-zero `region_size`, and
 * a non-zero `write_granularity` that evenly divides it (a real device that
 * cannot fill its own region with whole write units cannot use it safely).
 * Evenly dividing a positive `region_size` already implies `write_granularity
 * <= region_size` - a larger divisor cannot divide a smaller positive
 * dividend to a zero remainder - so that is not a separate condition here.
 *
 * boomlink_config_store.c layers its own format-specific minimums on top of
 * this - a `region_size` large enough for its header, an encoded config
 * that actually fits - since those are specific to its wrapper format (or,
 * for the config's encoded size, data-dependent), not to a generic port.
 */
bool boomlink_storage_port_is_valid(const boomlink_storage_port_t *port);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* BOOMLINK_STORAGE_PORT_H */
