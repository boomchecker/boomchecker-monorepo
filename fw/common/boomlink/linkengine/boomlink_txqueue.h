/**
 ******************************************************************************
 * @file    boomlink_txqueue.h
 * @brief   Bounded priority TX queue (boomlink.md section 9.8).
 *
 *          "At minimum use three logical priorities [...] low-priority telemetry
 *          must not block urgent traffic. The queue is statically bounded. When
 *          full, the drop policy should prefer dropping or coalescing
 *          low-priority telemetry before detection or command traffic."
 *
 *          A pure data structure: no radio, no clock, no frame encoding. It
 *          holds what to send and how urgently, and nothing about how. In
 *          particular it does NOT assign sequence numbers - section 9.1 assigns
 *          those at dequeue, precisely so that priority reordering here cannot
 *          make the on-air sequence non-monotonic and complicate the receiver's
 *          duplicate window (section 9.8's closing paragraph says so outright).
 ******************************************************************************
 */
#ifndef BOOMLINK_TXQUEUE_H
#define BOOMLINK_TXQUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "boomlink_linkframe.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Largest payload a queued frame can carry: the target radio's packet ceiling
   (RADIO_MAX_PAYLOAD, 255) minus the link frame header. Compile-time because the
   slots are statically allocated (agent rule 6); a port whose max_packet is
   SMALLER than this is not an error here - the engine checks the real limit when
   it sends, since only then is the port known. */
#define BOOMLINK_TX_MAX_PAYLOAD (255u - BOOMLINK_LINKFRAME_HEADER_SIZE)

/* Slot count. Sized for section 1.1's deployment (5-10 nodes, event-driven low
   rate), not for generality: at ~235 bytes of payload per slot this is already
   ~2 KB of static RAM, and a deeper queue on a link that can hold exactly one
   unacknowledged frame at a time (section 9.6, stop-and-wait) buys latency, not
   throughput. */
#define BOOMLINK_TXQUEUE_SLOTS 8u

/**
 * Section 9.8's three priorities. Ordered so that a numerically greater value is
 * more urgent, which makes "prefer dropping the lower priority" a plain `<`
 * rather than something a reader has to decode.
 *
 *   HIGH   ACK, command response, critical system message
 *   NORMAL detection events, configuration responses
 *   LOW    periodic telemetry, non-critical diagnostics
 *
 * ACKs are listed under HIGH by section 9.8, but in this implementation an ACK
 * never passes through this queue at all: it is sent immediately on the RX path,
 * because an ACK that waits behind queued traffic can easily miss the sender's
 * ACK timeout and provoke the retry it was meant to prevent.
 */
typedef enum {
  BOOMLINK_TXPRIO_LOW    = 0,
  BOOMLINK_TXPRIO_NORMAL = 1,
  BOOMLINK_TXPRIO_HIGH   = 2,
} boomlink_tx_priority_t;

#define BOOMLINK_TXPRIO_COUNT 3u

/** One queued transmission. Payload is held BY VALUE - see boomlink_txqueue_push(). */
typedef struct {
  uint32_t              destination_id;
  boomlink_tx_priority_t priority;
  /* What the caller asked for. The engine still overrides it to false for a
     broadcast destination (section 9.9: "broadcast never requests link ACK"),
     which is a TX-pipeline rule and so not enforced here. */
  bool                  ack_requested;
  uint8_t               payload[BOOMLINK_TX_MAX_PAYLOAD];
  size_t                payload_len;
} boomlink_txqueue_item_t;

typedef struct {
  struct {
    boomlink_txqueue_item_t item;
    bool                    used;
    /* Arrival stamp, for FIFO order within one priority. */
    uint32_t                arrived;
  } slots[BOOMLINK_TXQUEUE_SLOTS];
  uint32_t clock;
  /* Section 9.10 wants TX failures counted; these two separate the reasons,
     because they mean different things operationally. A rejected push means this
     node is generating more traffic than the link can carry; an eviction means
     it is shedding telemetry to protect detections, which is the policy working
     as intended rather than a fault. */
  uint32_t rejected;
  uint32_t evicted;
} boomlink_txqueue_t;

typedef enum {
  /* Queued. */
  BOOMLINK_TXQUEUE_OK = 0,
  /* Queued, and a strictly lower-priority item was dropped to make room. */
  BOOMLINK_TXQUEUE_OK_EVICTED,
  /* Not queued: the queue is full of items at least as urgent as this one. */
  BOOMLINK_TXQUEUE_FULL,
  /* Not queued: the payload cannot fit in a frame. */
  BOOMLINK_TXQUEUE_TOO_LONG,
} boomlink_txqueue_result_t;

/** Empty the queue. Must be called before first use. */
void boomlink_txqueue_init(boomlink_txqueue_t *queue);

/**
 * Queue `payload` for `destination_id` at `priority`.
 *
 * The payload is COPIED into the slot, so the caller's buffer need not outlive
 * the call. That costs static RAM and is the right trade for link code: a queue
 * of pointers would make every caller responsible for keeping a buffer alive
 * across an unbounded queueing delay plus up to three transmission attempts
 * (section 9.6), which is exactly the kind of lifetime rule that holds until the
 * one path that forgets it.
 *
 * When the queue is full, section 9.8's policy applies: the least urgent item is
 * dropped to make room, but only if it is STRICTLY less urgent than the incoming
 * one. So telemetry gives way to a detection event, a detection event does not
 * give way to another detection event, and a telemetry push into a queue full of
 * telemetry is refused rather than displacing an older reading that has been
 * waiting longer. Among equally unurgent candidates the OLDEST is dropped: for
 * the periodic telemetry this protects, the freshest reading is the useful one.
 *
 * @return see boomlink_txqueue_result_t. Both failure results leave the queue
 *         unchanged, and both are counted.
 */
boomlink_txqueue_result_t boomlink_txqueue_push(boomlink_txqueue_t *queue,
                                                uint32_t destination_id,
                                                boomlink_tx_priority_t priority,
                                                bool ack_requested, const uint8_t *payload,
                                                size_t payload_len);

/**
 * Remove and return the most urgent item, oldest first within a priority.
 *
 * Strict priority order, with a consequence worth stating: continuous urgent
 * traffic can starve LOW indefinitely. Section 9.8 requires that "low-priority
 * telemetry must not block urgent traffic" and says nothing about the reverse,
 * and at section 1.1's traffic rate a node cannot generate enough urgent traffic
 * to starve anything. If that ever changes, this is the function to revisit -
 * an aging bonus belongs here and nowhere else.
 *
 * @return false if the queue is empty, leaving *out_item untouched.
 */
bool boomlink_txqueue_pop(boomlink_txqueue_t *queue, boomlink_txqueue_item_t *out_item);

/** Total queued items. */
size_t boomlink_txqueue_count(const boomlink_txqueue_t *queue);

/** Queued items at exactly `priority`. */
size_t boomlink_txqueue_count_at(const boomlink_txqueue_t *queue,
                                 boomlink_tx_priority_t priority);

/** Pushes refused because the queue was full of equally-or-more urgent traffic. */
uint32_t boomlink_txqueue_rejected(const boomlink_txqueue_t *queue);

/** Items dropped to make room for more urgent traffic. */
uint32_t boomlink_txqueue_evicted(const boomlink_txqueue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* BOOMLINK_TXQUEUE_H */
