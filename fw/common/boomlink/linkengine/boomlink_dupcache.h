/**
 ******************************************************************************
 * @file    boomlink_dupcache.h
 * @brief   Bounded duplicate suppression (boomlink.md section 9.4).
 *
 *          "Retransmission can result in the same valid packet being received
 *          more than once. Application handlers must see an ACKed message at
 *          most once." A retry reuses the same (session_id, sequence) on purpose
 *          (section 9.6) precisely so the receiver can tell the second copy from
 *          a new message - this is what makes that possible.
 *
 *          A pure data structure with no radio, clock or engine dependency, so
 *          it can be tested exhaustively on its own: the interesting cases are
 *          sequence wrap, the edges of the reordering window, eviction, and a
 *          peer rebooting mid-conversation, and none of them needs a packet.
 ******************************************************************************
 */
#ifndef BOOMLINK_DUPCACHE_H
#define BOOMLINK_DUPCACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Section 9.4: "a statically allocated table of the N most recent
   (source_id, session_id) pairs (N = 16 is ample for 5-10 nodes)". Static, as
   agent rule 6 requires - no dynamic allocation in link code. */
#define BOOMLINK_DUPCACHE_ENTRIES 16u

/* Width of the reordering window kept below each entry's highest accepted
   sequence: section 9.4's "small bitmap window of recently accepted sequences
   below it (tolerates minor reordering)".
   32 rather than a larger number because of what the window is FOR. BoomLink v1
   is stop-and-wait (section 9.6): one ACK-pending frame at a time, globally, so
   a sender cannot have more than one unacknowledged frame in flight and genuine
   reordering is limited to a retry arriving after its own late ACK. 32 slots is
   already far more tolerance than that needs; the cost of more would be paid in
   RAM on every entry for cases this MAC cannot produce. */
#define BOOMLINK_DUPCACHE_WINDOW 32u

typedef struct {
  uint32_t source_id;
  uint32_t session_id;
  /* Highest sequence accepted from this (source, session), in the modular sense
     described in boomlink_dupcache_check(). */
  uint32_t highest_sequence;
  /* Bit i set means "highest_sequence - 1 - i has been accepted". Bit 0 is the
     sequence immediately below the highest. */
  uint32_t window;
  /* Recency stamp for eviction; 0 means the slot is unused. */
  uint32_t last_used;
} boomlink_dupcache_entry_t;

typedef struct {
  boomlink_dupcache_entry_t entries[BOOMLINK_DUPCACHE_ENTRIES];
  /* Incremented on every access to stamp the entry touched. */
  uint32_t                  clock;
  /* Diagnostics, for section 9.10's statistics and for tests that need to know a
     scenario actually exercised eviction rather than merely filling the table. */
  uint32_t                  evictions;
} boomlink_dupcache_t;

typedef enum {
  /* Not seen before; the caller should process it. Recorded, so the next copy
     will not be. */
  BOOMLINK_DUPCACHE_NEW = 0,
  /* Already accepted, or too old to prove otherwise - see below. Drop it, and
     resend the ACK if the frame asked for one (section 9.4). */
  BOOMLINK_DUPCACHE_DUPLICATE,
} boomlink_dupcache_result_t;

/** Empty the cache. Must be called before first use. */
void boomlink_dupcache_init(boomlink_dupcache_t *cache);

/**
 * Whether the frame identified by (source_id, session_id, sequence) - section
 * 9.3's packet identity - is new, recording it if so.
 *
 * Sequence comparison is MODULAR, not arithmetic. Section 9.3 requires that
 * "sequence wrap must be handled safely", and a plain `sequence >
 * highest_sequence` fails at the wrap in the worst possible way: after the
 * counter rolls over, every subsequent frame looks older than the highest ever
 * seen, so a node goes permanently deaf to that peer until it reboots. So
 * "after" means `(uint32_t)(a - b)` lies in 1..2^31-1, the serial-number
 * comparison from RFC 1982. The cost is that a jump of more than 2^31 is read as
 * a step backwards, which no sender can produce: at this traffic rate reaching
 * 2^31 sequences takes longer than the hardware will exist, and a reboot changes
 * the session_id rather than the sequence.
 *
 * A sequence older than the window is reported DUPLICATE rather than NEW. That
 * is a deliberate choice between two wrong answers - the cache cannot know
 * whether such a frame was already delivered - and it errs toward the guarantee
 * section 9.4 actually states: "application handlers must see an ACKed message
 * at most once". Accepting it would break that guarantee to salvage a frame so
 * stale the sender has long since given up on it, and would also make the link
 * replayable, which section 14 flags as a real concern.
 *
 * A frame from a source whose (source, session) pair is not in the table is
 * always NEW - including a peer that has rebooted, which section 9.3 makes
 * explicit by design ("a fresh session_id on reboot makes reboot behaviour
 * explicit"). An entry for the same source with a DIFFERENT session is replaced
 * rather than kept alongside: a node has exactly one current session, so keeping
 * the old one would spend a slot on a session that can never produce another
 * frame.
 */
boomlink_dupcache_result_t boomlink_dupcache_check(boomlink_dupcache_t *cache,
                                                   uint32_t source_id, uint32_t session_id,
                                                   uint32_t sequence);

/**
 * How many table slots are in use. For diagnostics and for tests that need to
 * distinguish "the table filled up" from "the table evicted something".
 */
size_t boomlink_dupcache_used(const boomlink_dupcache_t *cache);

/** How many entries have been evicted since init. */
uint32_t boomlink_dupcache_evictions(const boomlink_dupcache_t *cache);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* BOOMLINK_DUPCACHE_H */
