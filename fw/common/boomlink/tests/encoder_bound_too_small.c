/**
 ******************************************************************************
 * @file    encoder_bound_too_small.c
 * @brief   The NEGATIVE direction of boomlink_linkframe.h's C encoder bound:
 *          this file must FAIL to compile, diagnosed by -Wstringop-overflow
 *          (GCC) or -Warray-bounds (clang). tests/check_encoder_bound.sh
 *          compiles it and fails if it succeeds - or if it fails for any other
 *          reason.
 *
 *          Deliberately not part of any build target, since it cannot compile.
 ******************************************************************************
 */
#include "boomlink_linkframe.h"

int boomlink_c_bound_too_small(void);

int boomlink_c_bound_too_small(void) {
  boomlink_linkframe_header_t header = {
      .magic      = BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
      .version    = BOOMLINK_LINKFRAME_VERSION,
      .frame_type = BOOMLINK_FRAME_TYPE_DATA,
  };

  /* The case that motivates the bound: a short array INSIDE a larger object. A
     too-small standalone buffer is caught by AddressSanitizer at runtime, but
     the four bytes written past `frame` here land on `crc` - still inside a
     valid allocation, so ASan reports nothing whatsoever. The compile-time
     bound is the only thing that sees it. */
  struct {
    uint8_t  frame[BOOMLINK_LINKFRAME_HEADER_SIZE - 4u];
    uint32_t crc;
  } slot = {{0}, 0u};

  boomlink_linkframe_encode(&header, slot.frame);
  return (int)slot.crc;
}
