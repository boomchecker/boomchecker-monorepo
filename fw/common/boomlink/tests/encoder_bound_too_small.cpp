/**
 ******************************************************************************
 * @file    encoder_bound_too_small.cpp
 * @brief   The NEGATIVE direction of boomlink_linkframe.h's C++ bound: this file
 *          must FAIL to compile, with the header's static_assert as the reason.
 *          tests/check_encoder_bound.sh compiles it and fails if it succeeds -
 *          or if it fails for any other reason.
 *
 *          Deliberately not part of any build target, since it cannot compile.
 ******************************************************************************
 */
#include "boomlink_linkframe.h"

extern "C" int boomlink_cxx_bound_too_small(void);

int boomlink_cxx_bound_too_small(void) {
  boomlink_linkframe_header_t header = {};

  /* The case that motivates the whole construct: a short array INSIDE a larger
     object. A too-small standalone buffer is caught by AddressSanitizer at
     runtime, but writing past `hdr` here lands on `crc` - still inside a valid
     allocation, so ASan reports nothing at all. Only the compile-time bound
     sees it, and in C++ `[static N]` cannot provide one. */
  struct RxSlot {
    uint8_t  hdr[BOOMLINK_LINKFRAME_HEADER_SIZE - 4u];
    uint32_t crc;
  };
  RxSlot slot = {};
  boomlink_linkframe_encode(&header, slot.hdr);
  return static_cast<int>(slot.crc);
}
