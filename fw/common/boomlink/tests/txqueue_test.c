/**
 ******************************************************************************
 * @file    txqueue_test.c
 * @brief   Tests for boomlink_txqueue (boomlink.md section 9.8).
 *
 *          Two requirements are under test and they pull in opposite
 *          directions: "low-priority telemetry must not block urgent traffic",
 *          and a drop policy that sheds telemetry rather than detections. A
 *          queue that satisfies the first by dropping everything, or the second
 *          by never dropping anything, would pass a careless test of either one
 *          alone.
 ******************************************************************************
 */
#include <string.h>

#include "boomlink_txqueue.h"
#include "c_test.h"

BOOMLINK_TEST_STATE;

#define DEST 0x00000042u

static boomlink_txqueue_result_t push(boomlink_txqueue_t *q, boomlink_tx_priority_t prio,
                                      uint8_t tag) {
  const uint8_t payload[] = {tag, 0xAA, 0xBB};
  return boomlink_txqueue_push(q, DEST, prio, true, payload, sizeof(payload));
}

/** The tag byte of the next item, or -1 if the queue is empty. */
static int pop_tag(boomlink_txqueue_t *q) {
  boomlink_txqueue_item_t item;
  memset(&item, 0, sizeof(item));
  if (!boomlink_txqueue_pop(q, &item)) {
    return -1;
  }
  return item.payload_len > 0u ? (int)item.payload[0] : -2;
}

static void test_urgent_traffic_overtakes_queued_telemetry(void) {
  boomlink_txqueue_t q;
  boomlink_txqueue_init(&q);

  /* Telemetry queued first, then a detection, then a command response. Section
     9.8's core requirement: the telemetry already waiting must not delay them. */
  CHECK(push(&q, BOOMLINK_TXPRIO_LOW, 1u) == BOOMLINK_TXQUEUE_OK, "telemetry queued");
  CHECK(push(&q, BOOMLINK_TXPRIO_LOW, 2u) == BOOMLINK_TXQUEUE_OK, "more telemetry");
  CHECK(push(&q, BOOMLINK_TXPRIO_NORMAL, 3u) == BOOMLINK_TXQUEUE_OK, "a detection event");
  CHECK(push(&q, BOOMLINK_TXPRIO_HIGH, 4u) == BOOMLINK_TXQUEUE_OK, "a command response");

  CHECK(pop_tag(&q) == 4, "the HIGH item goes first");
  CHECK(pop_tag(&q) == 3, "then NORMAL");
  CHECK(pop_tag(&q) == 1, "then the OLDEST telemetry");
  CHECK(pop_tag(&q) == 2, "then the newer telemetry");
  CHECK(pop_tag(&q) == -1, "and the queue is empty");
}

static void test_order_within_one_priority_is_fifo(void) {
  boomlink_txqueue_t q;
  boomlink_txqueue_init(&q);

  /* Section 9.8: "priorities reorder only the queue". Within a priority the
     order must be arrival order - a LIFO here would deliver detection events out
     of order, and since section 9.1 assigns the sequence at dequeue the on-air
     sequence would still be monotonic, so nothing downstream would notice. */
  for (uint8_t i = 0; i < 5u; i++) {
    CHECK(push(&q, BOOMLINK_TXPRIO_NORMAL, (uint8_t)(10u + i)) == BOOMLINK_TXQUEUE_OK,
          "item %u", (unsigned)i);
  }
  for (uint8_t i = 0; i < 5u; i++) {
    CHECK(pop_tag(&q) == (int)(10u + i), "item %u must come out in arrival order",
          (unsigned)i);
  }
}

static void test_a_detection_displaces_telemetry_when_full(void) {
  boomlink_txqueue_t q;
  boomlink_txqueue_init(&q);

  for (uint8_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS; i++) {
    CHECK(push(&q, BOOMLINK_TXPRIO_LOW, (uint8_t)(100u + i)) == BOOMLINK_TXQUEUE_OK,
          "telemetry %u", (unsigned)i);
  }
  CHECK(boomlink_txqueue_count(&q) == BOOMLINK_TXQUEUE_SLOTS, "the queue is full");
  CHECK(boomlink_txqueue_evicted(&q) == 0u, "filling it evicted nothing");

  /* Section 9.8: "the drop policy should prefer dropping [...] low-priority
     telemetry before detection or command traffic". */
  CHECK(push(&q, BOOMLINK_TXPRIO_NORMAL, 200u) == BOOMLINK_TXQUEUE_OK_EVICTED,
        "a detection event must displace telemetry");
  CHECK(boomlink_txqueue_evicted(&q) == 1u, "exactly one item shed, got %u",
        (unsigned)boomlink_txqueue_evicted(&q));
  CHECK(boomlink_txqueue_count(&q) == BOOMLINK_TXQUEUE_SLOTS, "the queue stays full");
  CHECK(boomlink_txqueue_count_at(&q, BOOMLINK_TXPRIO_LOW) == BOOMLINK_TXQUEUE_SLOTS - 1u,
        "one telemetry item is gone");

  CHECK(pop_tag(&q) == 200, "the detection is sent first");
  /* The OLDEST telemetry was the victim, so the remaining readings are the
     freshest ones - which is the point for periodic telemetry. */
  CHECK(pop_tag(&q) == 101, "the oldest telemetry (100) was the one dropped");
}

static void test_equal_priority_does_not_displace(void) {
  boomlink_txqueue_t q;
  boomlink_txqueue_init(&q);

  for (uint8_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS; i++) {
    CHECK(push(&q, BOOMLINK_TXPRIO_NORMAL, (uint8_t)(50u + i)) == BOOMLINK_TXQUEUE_OK,
          "detection %u", (unsigned)i);
  }
  /* STRICTLY less urgent, or a burst of detections would keep throwing away
     earlier detections - losing exactly the traffic the policy exists to
     protect, and doing it silently. Refusing the new one at least tells the
     caller. */
  CHECK(push(&q, BOOMLINK_TXPRIO_NORMAL, 99u) == BOOMLINK_TXQUEUE_FULL,
        "a detection must not displace another detection");
  CHECK(boomlink_txqueue_evicted(&q) == 0u, "nothing was evicted");
  CHECK(boomlink_txqueue_rejected(&q) == 1u, "the refusal was counted");
  CHECK(pop_tag(&q) == 50, "the earliest detection is intact");

  /* And telemetry into a queue full of telemetry is refused rather than
     displacing the reading that has waited longest. */
  boomlink_txqueue_init(&q);
  for (uint8_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS; i++) {
    (void)push(&q, BOOMLINK_TXPRIO_LOW, (uint8_t)i);
  }
  CHECK(push(&q, BOOMLINK_TXPRIO_LOW, 77u) == BOOMLINK_TXQUEUE_FULL,
        "telemetry must not displace telemetry");
}

static void test_telemetry_cannot_displace_urgent_traffic(void) {
  boomlink_txqueue_t q;
  boomlink_txqueue_init(&q);

  for (uint8_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS; i++) {
    CHECK(push(&q, BOOMLINK_TXPRIO_HIGH, (uint8_t)(1u + i)) == BOOMLINK_TXQUEUE_OK,
          "urgent item %u", (unsigned)i);
  }
  /* The direction that would be catastrophic: telemetry evicting a command
     response. An eviction policy that looked only at "is the queue full" and
     dropped the oldest item would do exactly this. */
  CHECK(push(&q, BOOMLINK_TXPRIO_LOW, 250u) == BOOMLINK_TXQUEUE_FULL,
        "telemetry must never displace urgent traffic");
  CHECK(boomlink_txqueue_evicted(&q) == 0u, "and must evict nothing");
  CHECK(boomlink_txqueue_count_at(&q, BOOMLINK_TXPRIO_HIGH) == BOOMLINK_TXQUEUE_SLOTS,
        "every urgent item survived");
}

static void test_eviction_picks_by_priority_not_by_age(void) {
  boomlink_txqueue_t q;
  boomlink_txqueue_init(&q);

  /* A MIXED queue whose oldest item is the most urgent - which is the realistic
     shape, and the one that distinguishes "drop the least urgent" from "drop the
     oldest". Every other eviction case here happens to have the oldest item also
     be the least urgent, so a policy that ignored priority entirely passed all
     of them: the strict-priority guard turned the wrong victim into a refusal
     rather than a bad eviction, and a refusal looks the same as a full queue.
     Verified - replacing the candidate search with "oldest wins" left the whole
     suite green before this case existed.

     The consequence of that bug is the one section 9.8 exists to prevent, just
     arrived at sideways: a detection event is dropped while telemetry sits in
     the queue. */
  CHECK(push(&q, BOOMLINK_TXPRIO_HIGH, 1u) == BOOMLINK_TXQUEUE_OK,
        "the OLDEST item is urgent");
  for (uint8_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS - 2u; i++) {
    CHECK(push(&q, BOOMLINK_TXPRIO_NORMAL, (uint8_t)(20u + i)) == BOOMLINK_TXQUEUE_OK,
          "detection %u", (unsigned)i);
  }
  CHECK(push(&q, BOOMLINK_TXPRIO_LOW, 99u) == BOOMLINK_TXQUEUE_OK,
        "the NEWEST item is telemetry");
  CHECK(boomlink_txqueue_count(&q) == BOOMLINK_TXQUEUE_SLOTS, "full");

  CHECK(push(&q, BOOMLINK_TXPRIO_NORMAL, 30u) == BOOMLINK_TXQUEUE_OK_EVICTED,
        "a detection must be accepted by shedding the telemetry, not refused "
        "because the oldest item happens to outrank it");
  CHECK(boomlink_txqueue_count_at(&q, BOOMLINK_TXPRIO_LOW) == 0u,
        "the telemetry was the victim");
  CHECK(boomlink_txqueue_count_at(&q, BOOMLINK_TXPRIO_HIGH) == 1u,
        "the oldest item survived because it was the most urgent");
  CHECK(boomlink_txqueue_rejected(&q) == 0u, "nothing was refused");
  CHECK(pop_tag(&q) == 1, "and the urgent item is still first out");
}

static void test_payload_is_copied_not_referenced(void) {
  boomlink_txqueue_t q;
  boomlink_txqueue_init(&q);

  uint8_t scratch[8];
  memset(scratch, 0x11, sizeof(scratch));
  CHECK(boomlink_txqueue_push(&q, DEST, BOOMLINK_TXPRIO_NORMAL, true, scratch,
                              sizeof(scratch)) == BOOMLINK_TXQUEUE_OK,
        "queued");
  /* The caller's buffer is reused immediately - which is what a caller building
     frames in a scratch buffer will do. If the queue held a pointer, the item
     would now be whatever the caller overwrote it with, and the bug would only
     appear when the queue happened to be non-empty at the wrong moment. */
  memset(scratch, 0xEE, sizeof(scratch));

  boomlink_txqueue_item_t item;
  memset(&item, 0, sizeof(item));
  REQUIRE(boomlink_txqueue_pop(&q, &item), "the item should still be there");
  CHECK(item.payload_len == sizeof(scratch), "length preserved");
  for (size_t i = 0; i < sizeof(scratch); i++) {
    CHECK(item.payload[i] == 0x11u, "byte %zu is 0x%02X, expected the value at push time",
          i, item.payload[i]);
  }
}

static void test_the_fields_survive_the_round_trip(void) {
  boomlink_txqueue_t q;
  boomlink_txqueue_init(&q);

  const uint8_t payload[] = {1u, 2u, 3u, 4u};
  CHECK(boomlink_txqueue_push(&q, 0xDEADBEEFu, BOOMLINK_TXPRIO_HIGH, false, payload,
                              sizeof(payload)) == BOOMLINK_TXQUEUE_OK,
        "queued");
  boomlink_txqueue_item_t item;
  memset(&item, 0xFF, sizeof(item));
  REQUIRE(boomlink_txqueue_pop(&q, &item), "popped");
  CHECK(item.destination_id == 0xDEADBEEFu, "destination survived: got %08X",
        (unsigned)item.destination_id);
  CHECK(item.priority == BOOMLINK_TXPRIO_HIGH, "priority survived");
  /* ack_requested must come back as it went in. The engine overrides it for
     broadcast (section 9.9), but that is the engine's job, and a queue that
     silently forced it either way would hide that override or defeat it. */
  CHECK(item.ack_requested == false, "ack_requested survived as false");
  CHECK(item.payload_len == sizeof(payload), "length survived");
  CHECK(memcmp(item.payload, payload, sizeof(payload)) == 0, "payload survived");
}

static void test_an_oversize_payload_is_refused(void) {
  boomlink_txqueue_t q;
  boomlink_txqueue_init(&q);

  static uint8_t big[BOOMLINK_TX_MAX_PAYLOAD + 1u];
  memset(big, 0x5A, sizeof(big));
  CHECK(boomlink_txqueue_push(&q, DEST, BOOMLINK_TXPRIO_HIGH, true, big, sizeof(big)) ==
            BOOMLINK_TXQUEUE_TOO_LONG,
        "one byte past the payload limit must be refused, not truncated");
  CHECK(boomlink_txqueue_count(&q) == 0u, "and nothing queued");
  CHECK(boomlink_txqueue_rejected(&q) == 1u, "counted as a rejection");

  /* Exactly at the limit must be accepted, or the check is off by one in the
     other direction and the largest legal frame can never be sent. */
  CHECK(boomlink_txqueue_push(&q, DEST, BOOMLINK_TXPRIO_HIGH, true, big,
                              BOOMLINK_TX_MAX_PAYLOAD) == BOOMLINK_TXQUEUE_OK,
        "a payload of exactly the limit must be accepted");
  /* And an empty payload: section 9.2 lets a DATA frame carry no payload, and
     memcpy(dst, src, 0) with a valid pointer is fine, but a zero-length push is
     the kind of edge that gets special-cased into a rejection by accident. */
  CHECK(boomlink_txqueue_push(&q, DEST, BOOMLINK_TXPRIO_HIGH, true, big, 0u) ==
            BOOMLINK_TXQUEUE_OK,
        "an empty payload is legal");
}

static void test_an_empty_queue_yields_nothing(void) {
  boomlink_txqueue_t q;
  boomlink_txqueue_init(&q);

  boomlink_txqueue_item_t item;
  memset(&item, 0xC3, sizeof(item));
  CHECK(!boomlink_txqueue_pop(&q, &item), "popping an empty queue must fail");
  CHECK(item.destination_id == 0xC3C3C3C3u,
        "a failed pop must leave the caller's item untouched, so a caller that "
        "ignores the return value cannot act on a fabricated frame");
  CHECK(boomlink_txqueue_count(&q) == 0u, "still empty");

  /* Drain and re-fill, to catch a slot that was freed on pop but left marked
     used - which would shrink the queue permanently. */
  for (uint8_t round = 0; round < 3u; round++) {
    for (uint8_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS; i++) {
      CHECK(push(&q, BOOMLINK_TXPRIO_NORMAL, i) == BOOMLINK_TXQUEUE_OK,
            "round %u item %u", (unsigned)round, (unsigned)i);
    }
    for (uint8_t i = 0; i < BOOMLINK_TXQUEUE_SLOTS; i++) {
      CHECK(pop_tag(&q) == (int)i, "round %u item %u", (unsigned)round, (unsigned)i);
    }
    CHECK(boomlink_txqueue_count(&q) == 0u, "round %u drained fully", (unsigned)round);
  }
}

int main(void) {
  test_urgent_traffic_overtakes_queued_telemetry();
  test_order_within_one_priority_is_fifo();
  test_a_detection_displaces_telemetry_when_full();
  test_equal_priority_does_not_displace();
  test_telemetry_cannot_displace_urgent_traffic();
  test_eviction_picks_by_priority_not_by_age();
  test_payload_is_copied_not_referenced();
  test_the_fields_survive_the_round_trip();
  test_an_oversize_payload_is_refused();
  test_an_empty_queue_yields_nothing();
  BOOMLINK_TEST_REPORT("txqueue_test");
}
