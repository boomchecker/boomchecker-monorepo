/**
 ******************************************************************************
 * @file    boomlink_txqueue.c
 ******************************************************************************
 */
#include "boomlink_txqueue.h"

#include <string.h>

_Static_assert(BOOMLINK_TXQUEUE_SLOTS >= 1u, "a queue with no slots holds nothing");
_Static_assert(BOOMLINK_TX_MAX_PAYLOAD > 0u,
               "the header alone fills a packet - check RADIO_MAX_PAYLOAD");
_Static_assert(BOOMLINK_TXPRIO_HIGH > BOOMLINK_TXPRIO_NORMAL &&
                   BOOMLINK_TXPRIO_NORMAL > BOOMLINK_TXPRIO_LOW,
               "the priority order is compared with < and > throughout this file");

void boomlink_txqueue_init(boomlink_txqueue_t *queue) {
  memset(queue, 0, sizeof(*queue));
  queue->clock = 1u;
}

/** The slot holding the most urgent item, oldest first within a priority. */
static size_t find_next_to_send(const boomlink_txqueue_t *queue) {
  size_t best = BOOMLINK_TXQUEUE_SLOTS;
  for (size_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS; i++) {
    if (!queue->slots[i].used) {
      continue;
    }
    if (best == BOOMLINK_TXQUEUE_SLOTS) {
      best = i;
      continue;
    }
    if (queue->slots[i].item.priority > queue->slots[best].item.priority ||
        (queue->slots[i].item.priority == queue->slots[best].item.priority &&
         queue->slots[i].arrived < queue->slots[best].arrived)) {
      best = i;
    }
  }
  return best;
}

/** The slot holding the least urgent item, oldest first within a priority. */
static size_t find_eviction_candidate(const boomlink_txqueue_t *queue) {
  size_t worst = BOOMLINK_TXQUEUE_SLOTS;
  for (size_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS; i++) {
    if (!queue->slots[i].used) {
      continue;
    }
    if (worst == BOOMLINK_TXQUEUE_SLOTS) {
      worst = i;
      continue;
    }
    /* Least urgent wins; among equals the OLDEST, because the item this policy
       exists to shed is periodic telemetry, where the freshest reading is the
       useful one. */
    if (queue->slots[i].item.priority < queue->slots[worst].item.priority ||
        (queue->slots[i].item.priority == queue->slots[worst].item.priority &&
         queue->slots[i].arrived < queue->slots[worst].arrived)) {
      worst = i;
    }
  }
  return worst;
}

static size_t find_free_slot(const boomlink_txqueue_t *queue) {
  for (size_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS; i++) {
    if (!queue->slots[i].used) {
      return i;
    }
  }
  return BOOMLINK_TXQUEUE_SLOTS;
}

boomlink_txqueue_result_t boomlink_txqueue_push(boomlink_txqueue_t *queue,
                                                uint32_t destination_id,
                                                boomlink_tx_priority_t priority,
                                                bool ack_requested, const uint8_t *payload,
                                                size_t payload_len) {
  if (payload_len > BOOMLINK_TX_MAX_PAYLOAD) {
    /* Counted as a rejection: from the caller's point of view its message did
       not go out, which is the same operational fact as a full queue even though
       the cause differs. */
    queue->rejected++;
    return BOOMLINK_TXQUEUE_TOO_LONG;
  }

  bool   evicted = false;
  size_t slot    = find_free_slot(queue);
  if (slot == BOOMLINK_TXQUEUE_SLOTS) {
    const size_t victim = find_eviction_candidate(queue);
    /* STRICTLY less urgent, so equal priorities do not displace each other. A
       queue full of telemetry refuses more telemetry rather than dropping the
       reading that has waited longest - and a queue full of detections refuses a
       detection rather than losing one to make room for another. */
    if (victim == BOOMLINK_TXQUEUE_SLOTS || queue->slots[victim].item.priority >= priority) {
      queue->rejected++;
      return BOOMLINK_TXQUEUE_FULL;
    }
    queue->slots[victim].used = false;
    queue->evicted++;
    evicted = true;
    slot    = victim;
  }

  boomlink_txqueue_item_t *item = &queue->slots[slot].item;
  memset(item, 0, sizeof(*item));
  item->destination_id = destination_id;
  item->priority       = priority;
  item->ack_requested  = ack_requested;
  item->payload_len    = payload_len;
  if (payload_len > 0u) {
    memcpy(item->payload, payload, payload_len);
  }

  queue->slots[slot].used    = true;
  queue->slots[slot].arrived = queue->clock++;
  /* The stamp is 32-bit and only ever compared, so a wrap would invert FIFO
     order once, for one pair of items, costing a single reordering within one
     priority. At one increment per queued frame that is ~4.3 billion frames
     away, which this traffic rate does not reach in the hardware's lifetime -
     and the consequence is a latency artefact, not a lost or duplicated frame.
     Skipping 0 keeps the stamp distinguishable from a zeroed slot. */
  if (queue->clock == 0u) {
    queue->clock = 1u;
  }

  return evicted ? BOOMLINK_TXQUEUE_OK_EVICTED : BOOMLINK_TXQUEUE_OK;
}

bool boomlink_txqueue_pop(boomlink_txqueue_t *queue, boomlink_txqueue_item_t *out_item) {
  const size_t slot = find_next_to_send(queue);
  if (slot == BOOMLINK_TXQUEUE_SLOTS) {
    return false;
  }
  *out_item               = queue->slots[slot].item;
  queue->slots[slot].used = false;
  return true;
}

size_t boomlink_txqueue_count(const boomlink_txqueue_t *queue) {
  size_t count = 0u;
  for (size_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS; i++) {
    if (queue->slots[i].used) {
      count++;
    }
  }
  return count;
}

size_t boomlink_txqueue_count_at(const boomlink_txqueue_t *queue,
                                 boomlink_tx_priority_t priority) {
  size_t count = 0u;
  for (size_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS; i++) {
    if (queue->slots[i].used && queue->slots[i].item.priority == priority) {
      count++;
    }
  }
  return count;
}

uint32_t boomlink_txqueue_rejected(const boomlink_txqueue_t *queue) {
  return queue->rejected;
}

uint32_t boomlink_txqueue_evicted(const boomlink_txqueue_t *queue) {
  return queue->evicted;
}
