/**
 ******************************************************************************
 * @file    link_service.c
 ******************************************************************************
 */
#include "link_service.h"

#include "boomlink_radio_port.h"
#include "main.h" /* HAL_GetTick, HAL_GetUIDw0/1/2 */

/* Bring-up retry policy (section 9.6/9.7). Not final - like
   e22_radio::DefaultProfile(), these are reasonable-by-inspection defaults
   for lab bring-up, meant to be superseded by PR 4's persistent LinkConfig
   once a real deployment has measurements to tune them against.
   max_attempts matches section 9.6's own stated "default target: 3 total
   transmission attempts"; tx_jitter_max_ms matches section 9.7's own
   "a few tens of milliseconds of latency" for the cost it accepts. */
#define LINK_ACK_TIMEOUT_MARGIN_MS 50u
#define LINK_MAX_ATTEMPTS          3u
#define LINK_BACKOFF_MIN_MS        100u
#define LINK_BACKOFF_MAX_MS        400u
#define LINK_TX_JITTER_MAX_MS      50u

/* XORed into the session-id derivation only, so it does not come out
   numerically identical to boomlink_radio_port_init()'s PRNG seed - both mix
   the same three UID words with HAL_GetTick(), called moments apart in the
   same boot, and would otherwise coincide. Not a security property (section
   14 keeps that a separate concern); just not needlessly coupling two values
   that mean different things. */
#define LINK_SESSION_ID_SALT 0xA5A5A5A5u

static boomlink_link_t s_link;
static boomlink_port_t s_port;
static uint32_t        s_node_id;
static bool            s_initialized;
static bool            s_enabled = true;

/* 32-bit avalanche finalizer (Chris Wellons' "lowbias32", public domain) -
   a bijection on uint32_t with low measured bias, used below to combine the
   UID's three words into one better than a plain XOR would.

   Why a plain XOR of the three words isn't good enough: STM32's factory UID
   is structured (wafer X/Y coordinates plus a lot number), not uniformly
   random, so boards from the same production batch can share large spans of
   bits in the SAME word positions - which is exactly what XOR is blind to:
   two boards differing only in a couple of bits of one word, or differing
   in complementary bits across two words, can XOR-cancel into identical or
   near-identical combined values. Two boards with the same node_id cannot
   address each other at all, so this is worth a real mixing step rather
   than trusting three raw factory words. */
static uint32_t mix32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

/* This node's FALLBACK address (section 7.2), derived from the chip's
   factory-programmed 96-bit unique ID, for when link_service_init()'s
   configured_node_id is BOOMLINK_ADDR_INVALID/BOOMLINK_ADDR_BROADCAST -
   i.e. no persisted NodeConfig has ever assigned this node a real address.
   Before PR 4 Phase C wired NodeConfig into this file at all, this was the
   ONLY source of node_id, unconditionally, because no persisted config
   existed yet; a fixed constant would have made every board running this
   firmware the SAME node, unable to address each other at all. The UID
   still gives every board a distinct address with zero configuration for
   that unconfigured case, matching this codebase's "compliant-by-default"
   bring-up ethos elsewhere (e22_radio::DefaultProfile()). `link status` is
   how an operator reads the address actually in use back, whichever source
   it came from, to hand to a peer's `link ping <node_id>`.

   A real deployment assigns a persisted node_id through NodeConfig instead,
   which lets a ROLE (not a chip) own an address - the UID ties this
   fallback's identity to a specific board, which is wrong the moment a
   board is swapped in the field. Recorded here as a known bring-up-only
   limitation of the fallback path, not a design meant to carry forward.

   BOOMLINK_ADDR_INVALID (0) and BOOMLINK_ADDR_BROADCAST (0xFFFFFFFF) are
   both rejected by boomlink_link_init() (section 7.2's node_id contract) -
   astronomically unlikely for a well-mixed 32-bit value to land on either
   exact value, but cheap to guard rather than trust to chance. The two
   fallbacks are deliberately DISTINCT constants (not one shared value): two
   boards that separately happened to hit the two different forbidden values
   would otherwise collapse onto the same substitute and collide anyway. */
static uint32_t derive_node_id(void) {
  uint32_t id = mix32(HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2());
  if (id == BOOMLINK_ADDR_INVALID) {
    id = 1u;
  } else if (id == BOOMLINK_ADDR_BROADCAST) {
    id = 2u;
  }
  return id;
}

/* session_id (section 9.3): boomlink_link_init() refuses 0, and a
   microcontroller has no entropy at reset to draw a real one from before
   this file's own PRNG exists - so seed it from the same UID+tick mix
   boomlink_radio_port_init() uses for the port's PRNG (LINK_SESSION_ID_SALT
   keeps the two from coinciding - see above), which is already guaranteed
   non-zero there for the identical reason. Deliberately NOT the same
   derivation as node_id: node_id XORs the UID alone so it stays the SAME
   across reboots (section 7.2 wants identity stable); this folds in
   HAL_GetTick() too, which section 9.3 needs to CHANGE across reboots.

   It does not reliably do so, and that is a known, NOT-yet-fixed gap, not a
   theoretical one: HAL_GetTick() at this point in boot is the elapsed time
   since reset through a fixed sequence of MX_*_Init() calls and one
   constant-length HAL_Delay() (e22_radio::PowerUp()) - no user input, no
   variable-length wait, on a board whose radio answers (the healthy,
   common case). That makes this line read the SAME tick value on every
   single reboot, not rarely, which means the SAME session_id every reboot -
   exactly the failure boomlink_link_init()'s own doc warns against: a
   peer's duplicate cache silently suppresses this node's post-reboot frames
   as replays while still ACKing them, so a sender-side success (`link ping`
   reporting BOOMLINK_TX_ACKED) does not mean the payload reached the
   application. Flagged independently by two review passes; the fix is a
   flash-backed monotonic boot counter (Flash HAL is already enabled on this
   target, no new peripheral needed) rather than anything this function can
   compute alone - tracked as follow-up work, not fixed here. */
static uint32_t derive_session_id(void) {
  uint32_t id = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2() ^ HAL_GetTick() ^
                LINK_SESSION_ID_SALT;
  if (id == 0u) {
    id = 1u;
  }
  return id;
}

bool link_service_init(uint32_t configured_node_id, uint32_t configured_magic,
                       boomlink_link_rx_fn on_rx, void *on_rx_user,
                       boomlink_link_tx_done_fn on_tx_done, void *on_tx_done_user) {
  boomlink_radio_port_init(&s_port);

  /* BOOMLINK_ADDR_BROADCAST is included here defensively, not because a
     valid config can ever reach it: boomlink_config_service.c already
     rejects a SET attempting to CHANGE node_id to broadcast (section 7.2),
     so no config this file loads should legitimately carry it. Guarded
     anyway rather than trusted, for the same reason derive_node_id() below
     guards its own mixed value against both reserved constants instead of
     assuming a well-behaved input. */
  s_node_id = (configured_node_id == BOOMLINK_ADDR_INVALID ||
               configured_node_id == BOOMLINK_ADDR_BROADCAST)
                  ? derive_node_id()
                  : configured_node_id;

  /* configured_magic > 0xFFu cannot come from a config this file's own
     caller ever staged (boomlink_config_service.c rejects that at SET
     time too - section 7.3's one wire byte), but CAN come from a config
     blob written to flash before boomlink_node_config_defaults() carried
     a real default here - back when it left this field at its struct's
     zero-init. Falling back rather than truncating: a truncated value is
     still a real, silently-wrong magic two boards could disagree on. */
  uint8_t magic = (configured_magic == 0u || configured_magic > 0xFFu)
                      ? BOOMLINK_LINKFRAME_MAGIC_DEFAULT
                      : (uint8_t)configured_magic;

  boomlink_link_config_t config = {
    .node_id               = s_node_id,
    .magic                 = magic,
    .ack_timeout_margin_ms = LINK_ACK_TIMEOUT_MARGIN_MS,
    .max_attempts          = LINK_MAX_ATTEMPTS,
    .backoff_min_ms        = LINK_BACKOFF_MIN_MS,
    .backoff_max_ms        = LINK_BACKOFF_MAX_MS,
    .tx_jitter_max_ms      = LINK_TX_JITTER_MAX_MS,
    .on_rx                 = on_rx,
    .on_rx_user            = on_rx_user,
    .on_tx_done            = on_tx_done,
    .on_tx_done_user       = on_tx_done_user,
  };

  s_initialized = boomlink_link_init(&s_link, &config, &s_port, derive_session_id());
  return s_initialized;
}

void link_service_process(void) {
  if (!s_initialized || !s_enabled) {
    return;
  }
  boomlink_link_poll(&s_link);
}

bool link_service_enabled(void) {
  return s_enabled;
}

void link_service_set_enabled(bool enabled) {
  s_enabled = enabled;
}

uint32_t link_service_node_id(void) {
  return s_initialized ? s_node_id : 0u;
}

boomlink_link_t *link_service_link(void) {
  return s_initialized ? &s_link : NULL;
}
