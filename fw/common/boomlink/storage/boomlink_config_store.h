/**
 ******************************************************************************
 * @file    boomlink_config_store.h
 * @brief   PR 4 Phase B: persist a NodeConfig to flash with a small wrapper
 *          around the Nanopb blob (boomlink.md section 10.1), through the
 *          boomlink_storage_port_t seam so this file stays host-testable
 *          against a fake backend and Phase C's firmware call site wires the
 *          real one in.
 *
 *          Section 10.1's wrapper:
 *
 *              magic
 *              storage_format_version
 *              protobuf_length
 *              CRC
 *              serialized NodeConfig
 *
 *          A single fixed-size region, not a ping-pong pair of them: section
 *          10.1's own boot behaviour is "valid -> validate -> apply;
 *          missing/invalid -> load safe defaults", and a torn write (power
 *          lost mid-erase or mid-program) already falls into "invalid" under
 *          a single region exactly as it would under a double-buffered one -
 *          the CRC does not verify, defaults load, nothing is corrupted, the
 *          node just does not keep the config change it was in the middle of
 *          persisting. Section 10.1 does not ask for "never lose the most
 *          recent write", only for "never apply/use a corrupt one" - a
 *          second region buys wear-leveling and surviving a torn write
 *          without falling back to defaults, neither of which this section
 *          requires, at the cost of real complexity (a generation counter,
 *          picking the newer of two valid regions). Worth adding later if
 *          config writes turn out to be frequent enough for flash endurance
 *          to matter; guessing that now would be premature for a value that
 *          changes on operator action, not automatically.
 ******************************************************************************
 */
#ifndef BOOMLINK_CONFIG_STORE_H
#define BOOMLINK_CONFIG_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "boomlink_storage_port.h"
#include "config.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The on-disk header's serialized size: four uint32_t fields (magic,
 *  storage_format_version, protobuf_length, crc32), each written as four
 *  explicit little-endian bytes rather than overlaid as a struct - the same
 *  "byte-level, not struct-level" discipline linkframe/boomlink_linkframe.c
 *  uses for its own on-air header, for the same reason: this is a format
 *  read back by (potentially) a different compiler/build than wrote it
 *  across a firmware update, and struct layout is not part of that
 *  contract. */
#define BOOMLINK_CONFIG_STORE_HEADER_SIZE 16u

/** Not a guess: chosen so a corrupt-but-plausible region (all 0xFF from a
 *  fresh erase, or a stale different structure) is vanishingly unlikely to
 *  pass this check by accident, the same role BoomLink's own link frame
 *  magic (section 7.3) plays on the wire. */
#define BOOMLINK_CONFIG_STORE_MAGIC 0x424C4B43u /* ASCII "CKLB", read low byte first */

/** Bumped only if this WRAPPER's own layout changes (header field order/
 *  width) - never for a config.proto schema change, which Nanopb's own
 *  forward/backward compatibility already absorbs inside `protobuf_length`
 *  bytes without needing this file to know or care what changed. */
#define BOOMLINK_CONFIG_STORE_FORMAT_VERSION 1u

/** An upper bound on any port's write_granularity this file will ever be
 *  asked to support, so boomlink_config_store_save() can pad into a
 *  statically-sized local buffer instead of a variable-length one - this
 *  package has no heap (agent rule 6). STM32H5's real requirement is 16
 *  bytes (one quad-word); 64 leaves generous headroom for a future target
 *  with a coarser one without this file needing to change. Not enforced by
 *  boomlink_storage_port_is_valid() (a generic-port check, see its own doc -
 *  this constant is this wrapper format's own, not a generic port
 *  property); boomlink_config_store_save() checks `write_granularity`
 *  against this constant directly, up front, rather than only relying on
 *  the padded length it computes staying inside `sizeof` its local buffer -
 *  an earlier version of this file trusted that second check alone and was
 *  wrong to: `write_granularity` near SIZE_MAX makes the padding
 *  arithmetic (`total_len + write_granularity - 1u`) overflow to a small
 *  wrapped value, which sails straight past a bare `sizeof(buf)` check
 *  without ever being caught by it. */
#define BOOMLINK_CONFIG_STORE_MAX_WRITE_GRANULARITY 64u

/**
 * Load and validate the config persisted in `port`'s region into `*out`.
 *
 * Returns true only if the region's magic, storage_format_version and
 * protobuf_length are all structurally sound AND the CRC over the declared
 * blob matches AND that blob actually Nanopb-decodes as a NodeConfig -
 * section 10.1's "valid -> validate -> apply". Returns false for every
 * other case (a fresh/erased region, a corrupt one, a foreign one, a
 * decode failure) without distinguishing which - section 10.1's fallback is
 * the same regardless: "missing/invalid -> load safe defaults", so the
 * caller is expected to call boomlink_node_config_defaults() itself on a
 * false return, not this function.
 *
 * `*out` is left unmodified on a false return - never partially filled.
 *
 * Takes `boomlink_NodeConfig` (config.pb.h's generated type) directly rather
 * than services/boomlink_config_service.h's `boomlink_node_config_t` - they
 * are the same type (that header typedefs one to the other, see its own
 * doc), but this file has no need to include that header at all, and
 * storage staying independent of the config SERVICE's own header is worth
 * keeping true on purpose, not just true by accident of which type name got
 * used first.
 */
bool boomlink_config_store_load(const boomlink_storage_port_t *port, boomlink_NodeConfig *out);

/**
 * Persist `config` into `port`'s region: erase the whole region, then write
 * the header and the Nanopb-encoded blob as one padded buffer.
 *
 * "A configuration write must be validated before it becomes active or
 * persistent" (section 10.1) is this function's caller's job, not this
 * one's - by the time anything reaches here, boomlink_config_service.c has
 * already accepted the write (section 8.2's own validation, CONFIG_SET_
 * RESULT_INVALID, applies before this is ever called). This function's own
 * failure modes are purely mechanical: `config` too large to fit the
 * region, or the port's erase/write reporting a hardware error - both
 * reported as false, and the region's PRIOR contents may already be gone in
 * the second case (the erase already happened), which is exactly the same
 * "then look invalid on the next load" outcome as any other torn write -
 * see this file's own header doc for why that is an accepted failure mode,
 * not one this function papers over.
 */
bool boomlink_config_store_save(const boomlink_storage_port_t *port,
                                const boomlink_NodeConfig *config);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* BOOMLINK_CONFIG_STORE_H */
