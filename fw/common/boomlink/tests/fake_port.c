/**
 ******************************************************************************
 * @file    fake_port.c
 ******************************************************************************
 */
#include "fake_port.h"

#include <string.h>

void fake_air_init(fake_air_t *air) {
  memset(air, 0, sizeof(*air));
}

bool fake_air_ok(const fake_air_t *air) {
  return !air->overflowed;
}

static fake_transmission_t *air_append(fake_air_t *air) {
  if (air->count >= FAKE_PORT_MAX_TRANSMISSIONS) {
    /* Recorded rather than wrapped or aborted: a scenario that transmits more
       than the log holds must fail, because its assertions are about
       transmissions the log no longer has - but failing HERE would report the
       overflow from inside a callback, far from the scenario that caused it.
       fake_air_ok() is checked where the scenario ends. */
    air->overflowed = true;
    return NULL;
  }
  fake_transmission_t *tx = &air->log[air->count++];
  memset(tx, 0, sizeof(*tx));
  return tx;
}

static void air_put(fake_air_t *air, uint8_t sender, const uint8_t *bytes, size_t len) {
  fake_transmission_t *tx = air_append(air);
  if (tx == NULL) {
    return;
  }
  tx->sender   = sender;
  tx->wire_len = len;
  /* An oversize packet is truncated into the record but keeps its true length,
     so a scenario can present one and the engine still sees wire_len > cap the
     way a real driver would report it. */
  tx->len = len > FAKE_PORT_MAX_PACKET ? FAKE_PORT_MAX_PACKET : len;
  memcpy(tx->bytes, bytes, tx->len);
}

void fake_air_inject(fake_air_t *air, uint8_t sender, const uint8_t *bytes, size_t len) {
  air_put(air, sender, bytes, len);
}

const fake_transmission_t *fake_air_transmission(const fake_air_t *air, size_t index) {
  return index < air->count ? &air->log[index] : NULL;
}

size_t fake_air_count(const fake_air_t *air) {
  return air->count;
}

static int fake_send(void *ctx_v, const uint8_t *frame, size_t len) {
  fake_port_ctx_t *ctx = (fake_port_ctx_t *)ctx_v;
  ctx->send_calls++;
  if (ctx->send_result != 0) {
    return ctx->send_result;
  }
  if (ctx->swallow_tx) {
    /* Reported as accepted, never reaches the air. This is a LOST packet, and
       it has to look like success to the engine: a radio that reports failure
       is a different scenario (send_result), and conflating them would let a
       retry test pass against an engine that only retries on a send error. */
    return 0;
  }

  uint8_t staged[FAKE_PORT_MAX_PACKET];
  size_t  staged_len = len > sizeof(staged) ? sizeof(staged) : len;
  memcpy(staged, frame, staged_len);
  if (ctx->corrupt_byte_index >= 0 && (size_t)ctx->corrupt_byte_index < staged_len) {
    staged[ctx->corrupt_byte_index] = (uint8_t)(staged[ctx->corrupt_byte_index] ^ 0xFFu);
    ctx->corrupt_byte_index         = -1; /* one-shot */
  }
  air_put(ctx->air, ctx->node, staged, len);
  ctx->sends_on_air++;
  return 0;
}

static bool fake_poll_rx(void *ctx_v, uint8_t *buf, size_t cap, size_t *out_len,
                         float *out_rssi_dbm, float *out_snr_db) {
  fake_port_ctx_t *ctx = (fake_port_ctx_t *)ctx_v;
  while (ctx->rx_cursor < ctx->air->count) {
    fake_transmission_t *tx = &ctx->air->log[ctx->rx_cursor++];
    /* A half-duplex radio does not hear itself. Without this every engine would
       receive its own frames and its own ACKs, and a duplicate-suppression or
       ACK-matching test would be exercising self-delivery rather than a link. */
    if (tx->sender == ctx->node) {
      continue;
    }
    if (ctx->node < FAKE_PORT_MAX_NODES) {
      tx->delivered_to[ctx->node] = true;
    }
    size_t copied = tx->len < cap ? tx->len : cap;
    memcpy(buf, tx->bytes, copied);
    /* The TRUE length, not what fitted - the engine must be able to tell that a
       packet was longer than its buffer instead of silently accepting a prefix. */
    *out_len       = tx->wire_len;
    *out_rssi_dbm  = -95.5f;
    *out_snr_db    = 7.25f;
    return true;
  }
  return false;
}

static uint32_t fake_airtime_us(void *ctx_v, size_t len) {
  const fake_port_ctx_t *ctx = (const fake_port_ctx_t *)ctx_v;
  return (uint32_t)len * ctx->airtime_us_per_byte;
}

static uint32_t fake_now_ms(void *ctx_v) {
  return ((const fake_port_ctx_t *)ctx_v)->now_ms;
}

static uint32_t fake_random_u32(void *ctx_v) {
  fake_port_ctx_t *ctx = (fake_port_ctx_t *)ctx_v;
  /* xorshift32. Chosen for being three lines and fully deterministic from the
     seed, which is what a backoff-bounds test needs; it is not and need not be
     a good generator (section 14 keeps cryptography a separate concern). State
     must never be 0, which fake_port_init() guarantees. */
  uint32_t x = ctx->rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  ctx->rng_state = x;
  return x;
}

void fake_port_init(fake_port_ctx_t *ctx, fake_air_t *air, uint8_t node, uint32_t seed,
                    boomlink_port_t *out_port) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->air = air;
  ctx->node = node;
  /* xorshift32 is stuck at zero forever if seeded with zero, which would make
     every "random" backoff identical and a bounds test vacuous. Substituting a
     constant is safe here because the seed only has to be per-node distinct. */
  ctx->rng_state           = seed != 0u ? seed : 0xA5A5A5A5u;
  ctx->corrupt_byte_index  = -1;
  ctx->airtime_us_per_byte = 500u; /* ~10 ms for a bare header: SF9-ish, plausible */

  memset(out_port, 0, sizeof(*out_port));
  out_port->send        = fake_send;
  out_port->poll_rx     = fake_poll_rx;
  out_port->airtime_us  = fake_airtime_us;
  out_port->now_ms      = fake_now_ms;
  out_port->random_u32  = fake_random_u32;
  out_port->max_packet  = FAKE_PORT_MAX_PACKET;
  out_port->ctx         = ctx;
}

void fake_port_advance_ms(fake_port_ctx_t *ctx, uint32_t delta_ms) {
  ctx->now_ms += delta_ms;
}
