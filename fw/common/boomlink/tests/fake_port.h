/**
 ******************************************************************************
 * @file    fake_port.h
 * @brief   A fake boomlink_port_t: an in-memory radio medium, a clock that only
 *          moves when a test moves it, and a seeded PRNG.
 *
 *          This is what makes boomlink.md section 15.2's list testable at all -
 *          ACK matching, ACK timeout, retry count, duplicate suppression,
 *          duplicate ACK resend, sequence/session across a reboot, queue
 *          priority, queue overflow and randomized backoff bounds. Against real
 *          hardware none of them is a test: a timeout is a sleep, a retry count
 *          depends on whether a packet happened to be lost, and backoff bounds
 *          are unobservable.
 *
 *          Two properties are deliberate and load-bearing:
 *
 *          The clock NEVER advances on its own. Every scenario states the time
 *          it wants, so a test that passes does so because the engine's timing
 *          logic is right and not because a machine was fast enough. A wall
 *          clock would also make the retry tests flaky under CI load, which is
 *          the failure mode where a real bug gets re-run until it passes.
 *
 *          The medium is SHARED between nodes, and a node never hears its own
 *          transmissions (a half-duplex radio does not). So two engines can be
 *          stood up on one medium and exchange a frame and its real ACK, rather
 *          than each test simulating one side and asserting what the other side
 *          would have done - which is the shape that lets a build/match error
 *          cancel itself out.
 ******************************************************************************
 */
#ifndef BOOMLINK_FAKE_PORT_H
#define BOOMLINK_FAKE_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "boomlink_port.h"

/* Statically bounded, like everything else in this package (agent rule 6). 64
   transmissions is sized for the scenarios, not for generality: it covers the
   longest retry sequence with room to spare. Exceeding it is a hard failure
   rather than a silent wrap - a scenario that overflows the log is a scenario
   whose assertions would be meaningless.

   The packet cap is the port seam's own ceiling, not a second copy of 255: this
   fake has to be able to present exactly the packet a real radio at full
   capacity could, and one byte more than the engine will accept. */
#define FAKE_PORT_MAX_TRANSMISSIONS 64u
#define FAKE_PORT_MAX_PACKET        BOOMLINK_PORT_MAX_PACKET

typedef struct {
  uint8_t bytes[FAKE_PORT_MAX_PACKET];
  size_t  len;
  /* True length as transmitted, which may exceed `len` if a scenario
     deliberately injects an oversize packet - the engine must treat that as
     malformed rather than trust the truncated copy. */
  size_t  wire_len;
  uint8_t sender;
} fake_transmission_t;

/** The air. One per scenario; every node's port points at it. */
typedef struct {
  fake_transmission_t log[FAKE_PORT_MAX_TRANSMISSIONS];
  size_t              count;
  /* Set when a scenario transmits more than the log can hold. Checked by
     fake_air_ok() so an overflowing scenario fails loudly instead of quietly
     losing the transmissions its assertions are about. */
  bool                overflowed;
} fake_air_t;

typedef struct {
  fake_air_t *air;
  uint8_t     node;      /* index into the air's delivery bookkeeping */
  uint32_t    now_ms;
  uint32_t    rng_state;
  size_t      rx_cursor; /* next transmission this node has not yet considered */

  /* --- fault injection, all off by default --- */
  /* Returned by send() instead of 0. Non-zero makes every send fail the way a
     busy or absent radio does. */
  int  send_result;
  /* send() reports success but the frame never reaches the air: a lost packet,
     which is how an ACK is made to time out without touching the engine. */
  bool swallow_tx;
  /* Corrupt the Nth byte of the next transmission, for malformed-input paths.
     Negative disables. */
  int  corrupt_byte_index;
  /* Microseconds of airtime per byte the fake reports. Linear and fictional -
     the engine only ever scales and compares these, never treats them as
     physics - but non-zero so timeout derivation has something to derive from. */
  uint32_t airtime_us_per_byte;

  /* --- observation --- */
  size_t send_calls;    /* including failed and swallowed ones */
  size_t sends_on_air;  /* transmissions that actually reached the medium */
} fake_port_ctx_t;

/** Reset the air to empty. */
void fake_air_init(fake_air_t *air);

/** False if any scenario overflowed the transmission log. */
bool fake_air_ok(const fake_air_t *air);

/**
 * Initialise `ctx` for node index `node` on `air`, and fill `out_port` with the
 * callbacks bound to it. `seed` must differ per node: section 9.7's randomized
 * backoff exists to break up collisions, and two nodes drawing the same
 * sequence keeps colliding nodes in lockstep instead, making it worse.
 */
void fake_port_init(fake_port_ctx_t *ctx, fake_air_t *air, uint8_t node, uint32_t seed,
                    boomlink_port_t *out_port);

/** Move this node's clock forward. Nothing else advances time. */
void fake_port_advance_ms(fake_port_ctx_t *ctx, uint32_t delta_ms);

/**
 * Inject a raw packet into the air as if some other node had sent it, so a
 * scenario can present hostile or non-compliant traffic no engine would emit.
 * `sender` must not be this node's index or it will not be delivered.
 */
void fake_air_inject(fake_air_t *air, uint8_t sender, const uint8_t *bytes, size_t len);

/**
 * The `index`-th transmission that reached the air, or NULL. Lets a scenario
 * assert on the exact bytes rather than only on the engine's own counters -
 * which is the difference between checking what was sent and checking what the
 * sender believes it sent.
 */
const fake_transmission_t *fake_air_transmission(const fake_air_t *air, size_t index);

/** How many transmissions reached the air. */
size_t fake_air_count(const fake_air_t *air);

#endif /* BOOMLINK_FAKE_PORT_H */
