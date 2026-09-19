/**
 ******************************************************************************
 * @file    boomlink_dupcache.c
 ******************************************************************************
 */
#include "boomlink_dupcache.h"

#include <string.h>

_Static_assert(BOOMLINK_DUPCACHE_WINDOW <= 32u,
               "the window is a uint32_t bitmap, so it cannot exceed 32 slots");
_Static_assert(BOOMLINK_DUPCACHE_ENTRIES >= 1u, "a cache with no entries suppresses nothing");

/**
 * Whether `a` is after `b` in modular sequence order - RFC 1982 serial-number
 * comparison. See the header for why a plain `>` cannot be used and what this
 * gives up in exchange.
 *
 * Note this is strictly "after": equal is neither after nor before, which is
 * what makes the equality case in the caller distinct rather than folded into
 * one of the branches.
 */
static bool sequence_is_after(uint32_t a, uint32_t b) {
  const uint32_t delta = a - b;
  return delta != 0u && delta < 0x80000000u;
}

void boomlink_dupcache_init(boomlink_dupcache_t *cache) {
  memset(cache, 0, sizeof(*cache));
  /* last_used == 0 marks a slot unused, so the stamp counter must start above
     it: an entry stamped 0 on its first access would be indistinguishable from
     an empty slot and could be handed out again as free. */
  cache->clock = 1u;
}

/**
 * The entry for (source_id, session_id), creating one if needed.
 *
 * Never returns NULL: when the table is full the least recently used entry is
 * evicted, which is section 9.4's stated policy ("LRU eviction when the table is
 * full. A very stale retransmission from an evicted source may be delivered
 * twice - acceptable at this scale and traffic rate").
 */
static boomlink_dupcache_entry_t *find_or_create(boomlink_dupcache_t *cache,
                                                 uint32_t source_id, uint32_t session_id,
                                                 uint32_t sequence, bool *out_created) {
  boomlink_dupcache_entry_t *free_slot     = NULL;
  boomlink_dupcache_entry_t *same_source   = NULL;
  boomlink_dupcache_entry_t *lru           = NULL;

  for (size_t i = 0; i < BOOMLINK_DUPCACHE_ENTRIES; i++) {
    boomlink_dupcache_entry_t *entry = &cache->entries[i];
    if (entry->last_used == 0u) {
      if (free_slot == NULL) {
        free_slot = entry;
      }
      continue;
    }
    if (entry->source_id == source_id) {
      if (entry->session_id == session_id) {
        *out_created = false;
        return entry;
      }
      /* Same peer, different session: it rebooted (section 9.3). Its old session
         can never produce another frame, so the slot is reused rather than left
         to age out - otherwise a peer that reboots a few times evicts unrelated
         peers from the table with sessions that are already dead. */
      same_source = entry;
    }
    if (lru == NULL || entry->last_used < lru->last_used) {
      lru = entry;
    }
  }

  boomlink_dupcache_entry_t *target = same_source != NULL   ? same_source
                                      : free_slot != NULL   ? free_slot
                                                            : lru;
  if (target == lru && same_source == NULL && free_slot == NULL) {
    cache->evictions++;
  }

  target->source_id        = source_id;
  target->session_id       = session_id;
  target->highest_sequence = sequence;
  target->window           = 0u;
  *out_created             = true;
  return target;
}

boomlink_dupcache_result_t boomlink_dupcache_check(boomlink_dupcache_t *cache,
                                                   uint32_t source_id, uint32_t session_id,
                                                   uint32_t sequence) {
  bool                       created = false;
  boomlink_dupcache_entry_t *entry =
      find_or_create(cache, source_id, session_id, sequence, &created);

  /* Stamped on every access, hit or miss, so recency reflects traffic rather
     than only first contact - the LRU victim should be the peer that has been
     quiet longest, not the one that introduced itself longest ago.
     The stamp counter is 32-bit and never reset. At one packet per stamp it
     wraps after ~4.3 billion received frames, which this traffic rate does not
     reach in the hardware's lifetime; if it did, the only consequence is one
     eviction choosing a wrong victim, which costs at most one duplicate
     delivery - the same cost section 9.4 already accepts for eviction. */
  entry->last_used = cache->clock++;
  if (cache->clock == 0u) {
    cache->clock = 1u; /* never wrap onto the "unused" marker */
  }

  if (created) {
    /* First frame of this (source, session) - including the reboot case. */
    return BOOMLINK_DUPCACHE_NEW;
  }

  if (sequence == entry->highest_sequence) {
    return BOOMLINK_DUPCACHE_DUPLICATE;
  }

  if (sequence_is_after(sequence, entry->highest_sequence)) {
    const uint32_t advance = sequence - entry->highest_sequence;
    /* Three cases, and the two boundary ones are not decoration.
       The old highest must always end up recorded at bit advance-1: forgetting
       it is the classic sliding-window bug, where the very frame that moved the
       window becomes acceptable again and gets delivered twice.
       At advance == WINDOW that bit is still representable (bit 31), so the
       window must NOT simply be cleared - but `window << 32` on a uint32_t is
       undefined behaviour, not a zeroing shift, so the shift cannot be used
       either. Hence the explicit middle case. */
    if (advance > BOOMLINK_DUPCACHE_WINDOW) {
      entry->window = 0u;
    } else if (advance == BOOMLINK_DUPCACHE_WINDOW) {
      entry->window = 1u << (BOOMLINK_DUPCACHE_WINDOW - 1u);
    } else {
      entry->window = (entry->window << advance) | (1u << (advance - 1u));
    }
    entry->highest_sequence = sequence;
    return BOOMLINK_DUPCACHE_NEW;
  }

  /* Before the highest: inside the window it is a real duplicate check, outside
     it we cannot know - see the header for why the unknown case is reported as a
     duplicate rather than accepted. */
  const uint32_t age = entry->highest_sequence - sequence;
  if (age > BOOMLINK_DUPCACHE_WINDOW) {
    return BOOMLINK_DUPCACHE_DUPLICATE;
  }
  const uint32_t bit = 1u << (age - 1u);
  if ((entry->window & bit) != 0u) {
    return BOOMLINK_DUPCACHE_DUPLICATE;
  }
  entry->window |= bit;
  return BOOMLINK_DUPCACHE_NEW;
}

size_t boomlink_dupcache_used(const boomlink_dupcache_t *cache) {
  size_t used = 0u;
  for (size_t i = 0; i < BOOMLINK_DUPCACHE_ENTRIES; i++) {
    if (cache->entries[i].last_used != 0u) {
      used++;
    }
  }
  return used;
}

uint32_t boomlink_dupcache_evictions(const boomlink_dupcache_t *cache) {
  return cache->evictions;
}
