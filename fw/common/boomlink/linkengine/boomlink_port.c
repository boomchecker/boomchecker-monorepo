/**
 ******************************************************************************
 * @file    boomlink_port.c
 ******************************************************************************
 */
#include "boomlink_port.h"

#include "boomlink_linkframe.h"

bool boomlink_port_is_valid(const boomlink_port_t *port) {
  if (port == NULL) {
    return false;
  }
  if (port->send == NULL || port->poll_rx == NULL || port->airtime_us == NULL ||
      port->now_ms == NULL || port->random_u32 == NULL) {
    return false;
  }
  /* A radio that cannot carry a bare header cannot carry anything, so this is
     not an arbitrary floor: it is the point below which every send would fail
     and the link would be silently dead rather than misconfigured. */
  if (port->max_packet < BOOMLINK_LINKFRAME_HEADER_SIZE) {
    return false;
  }
  return true;
}

uint32_t boomlink_elapsed_ms(uint32_t earlier, uint32_t now) {
  /* Unsigned arithmetic is modular by definition (C17 6.2.5p9: unsigned
     operations "cannot overflow ... reduced modulo the number that is one
     greater than the largest value"), so this single subtraction is already
     correct across a wrap - now = 0x00000005 with earlier = 0xFFFFFFFF gives 6.
     The value of the function is that the contract is written down and tested
     once, instead of the idiom being re-derived at each call site with a chance
     of picking a signed or wider type, where it is wrong. */
  return now - earlier;
}
