/**
 ******************************************************************************
 * @file    boomlink_radio_port.c
 ******************************************************************************
 */
#include "boomlink_radio_port.h"

#include "main.h" /* HAL_GetTick, HAL_GetUIDw0/1/2 */
#include "radio.h"

/* xorshift32 (Marsaglia) state for random_u32. boomlink_port.h is explicit
   that quality requirements are low here - section 9.7's collision avoidance
   between a handful of nodes, not cryptography (section 14 keeps that a
   separate concern) - and the one real requirement is that two nodes must
   not draw the SAME sequence. STM32H5's hardware RNG peripheral would
   satisfy this trivially but is not enabled (see
   Core/Inc/stm32h5xx_hal_conf.h) and turning it on - clock tree, CubeMX
   regeneration - is more than this need justifies; a PRNG seeded per-node is
   enough. */
static uint32_t s_prng_state;

static uint32_t next_u32(void) {
  uint32_t x = s_prng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  s_prng_state = x;
  return x;
}

static int port_send(void *ctx, const uint8_t *frame, size_t len) {
  (void)ctx;
  return radio_send(frame, len);
}

static bool port_poll_rx(void *ctx, uint8_t *buf, size_t cap, size_t *out_len,
                          float *out_rssi_dbm, float *out_snr_db) {
  (void)ctx;
  return radio_poll_rx(buf, cap, out_len, out_rssi_dbm, out_snr_db);
}

static uint32_t port_airtime_us(void *ctx, size_t len) {
  (void)ctx;
  return radio_airtime_us(len);
}

static uint32_t port_now_ms(void *ctx) {
  (void)ctx;
  return HAL_GetTick();
}

static uint32_t port_random_u32(void *ctx) {
  (void)ctx;
  return next_u32();
}

void boomlink_radio_port_init(boomlink_port_t *out) {
  if (out == NULL) {
    return;
  }

  /* Seeded once, from this chip's factory-programmed 96-bit unique ID XORed
     with the tick at call time - two boards running IDENTICAL firmware (same
     static seed formula) then draw different sequences AS LONG AS the tick
     differs between them, without needing HAL_RNG wired up. Never re-seeded:
     boomlink_port.h asks for a per-node sequence, not a per-call one, and
     xorshift32's own period already covers a boot's worth of draws.

     Deliberately NOT given link_service.c's mix32() avalanche treatment
     (added there after review found the UID's plain XOR too weak against
     STM32's structured, non-random UID layout): the two callers have
     different failure costs. Two nodes with the same node_id cannot address
     each other at all - a hard failure, worth a real mixing step. Two nodes
     that happen to draw the same backoff/jitter sequence just get worse
     collision-avoidance for that one coincidence (boomlink_port.h's own
     "quality requirements are low here... not cryptography") - a soft
     degradation, not a correctness break, and this file's UID+tick mix
     already carries the same caveat link_service.c's derive_session_id()
     documents for the identical formula: on a board with deterministic boot
     timing, the tick term can fail to vary, so two same-batch boards booting
     in the same power event could plausibly draw the same seed too. Left
     as-is rather than "fixed" here because the fix (making the tick term
     actually vary) is section 9.3's session_id problem, tracked in issue
     #91 - duplicating that fix's effort here without the real fix behind it
     would just be two places pretending the same known gap is closed.

     xorshift32 has exactly one degenerate input - an all-zero state stays
     zero forever - which the UID/tick combination will not produce in
     practice, but is cheap to rule out rather than trust to chance. */
  s_prng_state = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2() ^ HAL_GetTick();
  if (s_prng_state == 0u) {
    s_prng_state = 0x9E3779B9u;
  }

  out->send       = port_send;
  out->poll_rx    = port_poll_rx;
  out->airtime_us = port_airtime_us;
  out->now_ms     = port_now_ms;
  out->random_u32 = port_random_u32;
  out->max_packet = RADIO_MAX_PAYLOAD;
  out->ctx        = NULL;
}
