/**
 ******************************************************************************
 * @file    cxx_bound_ok.cpp
 * @brief   The C++ half of boomlink_linkframe.h, compiled by
 *          tests/check_cxx_bound.sh as the POSITIVE direction: a correctly-sized
 *          buffer must compile clean, and the emitted call must be to the plain
 *          C symbol.
 *
 *          Nothing else in either build compiles this header as C++ yet - the
 *          firmware's C++ radio layer is what will, in a later phase - so
 *          without this the template wrapper and its `extern "C"` namespace
 *          trick could stop working (or stop being reachable) with nothing
 *          turning red. See cxx_bound_too_small.cpp for the other direction.
 ******************************************************************************
 */
#include "boomlink_linkframe.h"

/* Shaped like the eventual caller: encode into a radio TX buffer that is much
   larger than a header, then hand the payload region to the codec. */
extern "C" int boomlink_cxx_bound_ok(void);

int boomlink_cxx_bound_ok(void) {
  boomlink_linkframe_header_t header = {};
  header.magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT;
  header.version        = BOOMLINK_LINKFRAME_VERSION;
  header.frame_type     = BOOMLINK_FRAME_TYPE_DATA;
  header.ack_requested  = true;
  header.destination_id = BOOMLINK_ADDR_BROADCAST;

  uint8_t tx[255];
  boomlink_linkframe_encode(&header, tx);

  /* Exactly-sized is the boundary case and must also be accepted. */
  uint8_t exact[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&header, exact);

  /* The parse side has no array bound to restore, but it must still be callable
     from C++ - it takes `[static 1]` parameters, which are C-only spellings that
     have to degrade rather than fail here. */
  boomlink_linkframe_header_t decoded     = {};
  size_t                      payload_len = 0u;
  const boomlink_linkframe_parse_result_t result = boomlink_linkframe_parse(
      exact, sizeof(exact), BOOMLINK_LINKFRAME_MAGIC_DEFAULT, &decoded, &payload_len);
  if (result != BOOMLINK_LINKFRAME_OK ||
      !boomlink_linkframe_is_for_node(decoded.destination_id, 0x42u)) {
    return 1;
  }
  return tx[1] == exact[1] ? 0 : 1;
}
