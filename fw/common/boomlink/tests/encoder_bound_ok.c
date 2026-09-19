/**
 ******************************************************************************
 * @file    encoder_bound_ok.c
 * @brief   The C half of boomlink_linkframe.h's encoder bound, compiled by
 *          tests/check_encoder_bound.sh as the POSITIVE direction: a
 *          correctly-sized buffer must compile clean at every optimization
 *          level, warnings included.
 *
 *          `uint8_t out[static 20]` is a promise the callee always writes 20
 *          bytes, and GCC/clang enforce it against the caller. That makes a
 *          false positive on a legitimate caller a real risk - and the
 *          workaround people reach for when a bound cries wolf is to delete it -
 *          so the accepted cases are pinned here alongside the rejected one in
 *          encoder_bound_too_small.c.
 *
 *          Not part of any build target: the check script compiles it directly,
 *          because the point is the diagnostic, not the object file.
 ******************************************************************************
 */
#include "boomlink_linkframe.h"

int boomlink_c_bound_ok(void);

int boomlink_c_bound_ok(void) {
  boomlink_linkframe_header_t header = {
      .magic          = BOOMLINK_LINKFRAME_MAGIC_DEFAULT,
      .version        = BOOMLINK_LINKFRAME_VERSION,
      .frame_type     = BOOMLINK_FRAME_TYPE_DATA,
      .ack_requested  = true,
      .destination_id = BOOMLINK_ADDR_BROADCAST,
  };

  /* Shaped like the eventual caller: a radio TX buffer much larger than a
     header, encoded into at offset 0. */
  uint8_t tx[255];
  boomlink_linkframe_encode(&header, tx);

  /* Exactly-sized is the boundary case and must also be accepted. */
  uint8_t exact[BOOMLINK_LINKFRAME_HEADER_SIZE];
  boomlink_linkframe_encode(&header, exact);

  /* A large-enough array INSIDE a struct must be accepted too. This is the same
     shape as the rejected case in encoder_bound_too_small.c, differing only in
     the array's size - so if the bound ever started rejecting on the basis of
     "the object has a neighbour" rather than on the array's length, this fails
     rather than the whole construct quietly becoming useless. */
  struct {
    uint8_t  frame[BOOMLINK_LINKFRAME_HEADER_SIZE];
    uint32_t crc;
  } slot = {{0}, 0u};
  boomlink_linkframe_encode(&header, slot.frame);

  /* A pointer, where no bound can be checked at all. Accepted by design: the
     caller has taken responsibility, and `[static N]` says nothing about it.
     Pinned so the difference between "unchecked" and "rejected" stays
     deliberate. */
  uint8_t *via_pointer = tx;
  boomlink_linkframe_encode(&header, via_pointer);

  return tx[1] == exact[1] && exact[1] == slot.frame[1] ? 0 : 1;
}
