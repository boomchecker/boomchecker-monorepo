/**
 ******************************************************************************
 * @file    dupcache_test.c
 * @brief   Tests for boomlink_dupcache (boomlink.md section 9.4).
 *
 *          The guarantee under test is section 9.4's: "application handlers must
 *          see an ACKed message at most once". Every scenario here is a way that
 *          guarantee gets broken quietly - a wrap that makes a node deaf, a
 *          window edge that lets the frame which moved the window through a
 *          second time, a reboot that looks like a replay, an eviction that
 *          reopens a peer.
 ******************************************************************************
 */
#include "boomlink_dupcache.h"
#include "c_test.h"

BOOMLINK_TEST_STATE;

#define SRC_A 0x00000011u
#define SRC_B 0x00000022u
#define SESS  0xAABBCCDDu

static bool is_new(boomlink_dupcache_t *c, uint32_t src, uint32_t sess, uint32_t seq) {
  return boomlink_dupcache_check(c, src, sess, seq) == BOOMLINK_DUPCACHE_NEW;
}

static void test_a_retransmission_is_suppressed(void) {
  boomlink_dupcache_t c;
  boomlink_dupcache_init(&c);

  CHECK(is_new(&c, SRC_A, SESS, 1u), "the first frame is new");
  /* The case section 9.6 creates on purpose: a retry reuses the SAME
     (session_id, sequence) so this can recognise it. */
  CHECK(!is_new(&c, SRC_A, SESS, 1u), "the same frame again is a duplicate");
  CHECK(!is_new(&c, SRC_A, SESS, 1u), "and still is on the third copy");
  CHECK(is_new(&c, SRC_A, SESS, 2u), "the next sequence is new");
}

static void test_peers_are_tracked_independently(void) {
  boomlink_dupcache_t c;
  boomlink_dupcache_init(&c);

  CHECK(is_new(&c, SRC_A, SESS, 7u), "A's frame 7 is new");
  /* Identity is (source_id, session_id, sequence) per section 9.3, so the same
     sequence number from a different peer is a different frame. Without the
     source in the key, two nodes counting from 1 would suppress each other's
     traffic - and at this scale every node starts at 1 after every reboot. */
  CHECK(is_new(&c, SRC_B, SESS, 7u), "B's frame 7 is a different frame");
  CHECK(!is_new(&c, SRC_A, SESS, 7u), "A's frame 7 is still a duplicate");
  CHECK(!is_new(&c, SRC_B, SESS, 7u), "B's frame 7 is now a duplicate too");
  CHECK(boomlink_dupcache_used(&c) == 2u, "two peers, two entries");
}

static void test_a_reboot_is_not_a_replay(void) {
  boomlink_dupcache_t c;
  boomlink_dupcache_init(&c);

  for (uint32_t seq = 1u; seq <= 5u; seq++) {
    CHECK(is_new(&c, SRC_A, SESS, seq), "frame %u before the reboot", (unsigned)seq);
  }
  /* Section 9.3: "a fresh session_id on reboot makes reboot behaviour explicit".
     After rebooting, the peer's sequence restarts at 1 - which without the
     session in the key would look exactly like a replay of frames this node has
     already seen, and the peer would be unable to talk to it at all until its
     sequence climbed past 5. */
  const uint32_t new_session = SESS ^ 0x5A5A5A5Au;
  for (uint32_t seq = 1u; seq <= 5u; seq++) {
    CHECK(is_new(&c, SRC_A, new_session, seq), "frame %u after the reboot must be new",
          (unsigned)seq);
  }
  /* And the dead session is not kept alongside the live one: it can never
     produce another frame, so holding a slot for it would let a peer that
     reboots a few times evict unrelated peers. */
  CHECK(boomlink_dupcache_used(&c) == 1u,
        "the rebooted peer should occupy one entry, not two (used=%zu)",
        boomlink_dupcache_used(&c));
}

static void test_sequence_wrap_does_not_deafen_a_node(void) {
  boomlink_dupcache_t c;
  boomlink_dupcache_init(&c);

  CHECK(is_new(&c, SRC_A, SESS, 0xFFFFFFFEu), "a frame just below the wrap");
  CHECK(is_new(&c, SRC_A, SESS, 0xFFFFFFFFu), "the last sequence before the wrap");
  /* THE case section 9.3 means by "sequence wrap must be handled safely". With
     an arithmetic `sequence > highest` comparison, every frame after the rollover
     looks older than 0xFFFFFFFF forever, so the receiver goes permanently deaf to
     this peer until one of them reboots. Modular comparison makes 0 the
     successor of 0xFFFFFFFF. */
  CHECK(is_new(&c, SRC_A, SESS, 0u), "sequence 0 follows 0xFFFFFFFF and must be new");
  CHECK(is_new(&c, SRC_A, SESS, 1u), "and 1 after it");
  CHECK(is_new(&c, SRC_A, SESS, 2u), "and 2");
  /* Duplicate suppression must keep working across the boundary in both
     directions, or the wrap fix would have bought deafness for double delivery. */
  CHECK(!is_new(&c, SRC_A, SESS, 0u), "0 is now a duplicate");
  CHECK(!is_new(&c, SRC_A, SESS, 0xFFFFFFFFu),
        "the pre-wrap frame is still a duplicate afterwards");
  CHECK(!is_new(&c, SRC_A, SESS, 1u), "1 is a duplicate");
}

static void test_reordering_inside_the_window(void) {
  boomlink_dupcache_t c;
  boomlink_dupcache_init(&c);

  CHECK(is_new(&c, SRC_A, SESS, 100u), "establish the highest");
  CHECK(is_new(&c, SRC_A, SESS, 105u), "a forward jump");
  /* Section 9.4 wants the window to "tolerate minor reordering": the frames
     skipped over are not duplicates, they simply have not arrived yet. */
  CHECK(is_new(&c, SRC_A, SESS, 102u), "a frame that arrived late is still new");
  CHECK(is_new(&c, SRC_A, SESS, 101u), "and another");
  CHECK(!is_new(&c, SRC_A, SESS, 102u), "but only once");
  CHECK(!is_new(&c, SRC_A, SESS, 105u), "the highest is still a duplicate");
  CHECK(is_new(&c, SRC_A, SESS, 103u), "the remaining gap is still open");
  CHECK(!is_new(&c, SRC_A, SESS, 103u), "and closes after use");
}

static void test_the_frame_that_moved_the_window_cannot_return(void) {
  boomlink_dupcache_t c;
  boomlink_dupcache_init(&c);

  /* The classic sliding-window bug: shifting the history without marking the
     OLD highest as accepted leaves it acceptable again, so the frame that moved
     the window is delivered twice. Checked at three advances, including both
     boundaries - at advance == WINDOW the bit is still representable (bit 31) so
     the window must not be cleared, and at advance == WINDOW the shift itself
     would be undefined behaviour on a uint32_t. */
  const uint32_t advances[] = {1u, 2u, BOOMLINK_DUPCACHE_WINDOW - 1u,
                               BOOMLINK_DUPCACHE_WINDOW};
  for (size_t i = 0; i < sizeof(advances) / sizeof(advances[0]); i++) {
    const uint32_t advance = advances[i];
    boomlink_dupcache_init(&c);
    const uint32_t base = 1000u;
    CHECK(is_new(&c, SRC_A, SESS, base), "advance %u: the base frame", (unsigned)advance);
    CHECK(is_new(&c, SRC_A, SESS, base + advance), "advance %u: the moving frame",
          (unsigned)advance);
    CHECK(!is_new(&c, SRC_A, SESS, base),
          "advance %u: the frame that moved the window came back as NEW",
          (unsigned)advance);
    CHECK(!is_new(&c, SRC_A, SESS, base + advance),
          "advance %u: the new highest is a duplicate of itself", (unsigned)advance);
  }
}

static void test_a_jump_past_the_window_forgets_only_what_it_must(void) {
  boomlink_dupcache_t c;
  boomlink_dupcache_init(&c);

  const uint32_t base = 500u;
  CHECK(is_new(&c, SRC_A, SESS, base), "the base frame");
  /* One past the window: the old highest is no longer representable, so the
     window is cleared and that frame becomes acceptable again. That is the
     documented cost of a bounded window, not a bug - but it must happen only
     PAST the boundary, which is what the case above pins. */
  const uint32_t far = base + BOOMLINK_DUPCACHE_WINDOW + 1u;
  CHECK(is_new(&c, SRC_A, SESS, far), "a jump past the window");
  CHECK(!is_new(&c, SRC_A, SESS, far), "the new highest is recorded");
  /* And a frame older than the window is refused rather than accepted: the cache
     cannot know whether it was delivered, and section 9.4's guarantee is "at most
     once", so the unknown case must not be handed to the application. */
  CHECK(!is_new(&c, SRC_A, SESS, far - BOOMLINK_DUPCACHE_WINDOW - 1u),
        "a frame older than the window must not be accepted");
  CHECK(is_new(&c, SRC_A, SESS, far - BOOMLINK_DUPCACHE_WINDOW),
        "the oldest slot still inside the window is usable");
}

static void test_eviction_is_least_recently_used(void) {
  boomlink_dupcache_t c;
  boomlink_dupcache_init(&c);

  /* Fill every slot. */
  for (uint32_t i = 0; i < BOOMLINK_DUPCACHE_ENTRIES; i++) {
    CHECK(is_new(&c, 0x1000u + i, SESS, 1u), "peer %u's first frame", (unsigned)i);
  }
  CHECK(boomlink_dupcache_used(&c) == BOOMLINK_DUPCACHE_ENTRIES, "the table is full");
  CHECK(boomlink_dupcache_evictions(&c) == 0u, "filling the table is not eviction");

  /* Keep peer 0 active, so recency reflects TRAFFIC and not first contact. If
     the stamp were only set when an entry is created, the busiest peer would be
     evicted for being the oldest acquaintance. */
  CHECK(!is_new(&c, 0x1000u, SESS, 1u), "peer 0 is still talking (a duplicate)");

  /* A new peer must now evict someone - and it must be peer 1, the one that has
     been quiet longest, not peer 0. */
  CHECK(is_new(&c, 0x9999u, SESS, 1u), "a new peer arrives");
  CHECK(boomlink_dupcache_evictions(&c) == 1u, "exactly one eviction, got %u",
        (unsigned)boomlink_dupcache_evictions(&c));
  CHECK(boomlink_dupcache_used(&c) == BOOMLINK_DUPCACHE_ENTRIES,
        "the table stays full, it does not grow");
  CHECK(!is_new(&c, 0x1000u, SESS, 1u),
        "peer 0 must have survived - it was the most recently active");
  CHECK(is_new(&c, 0x1001u, SESS, 1u),
        "peer 1 was the least recently used and should have been the victim");
}

int main(void) {
  test_a_retransmission_is_suppressed();
  test_peers_are_tracked_independently();
  test_a_reboot_is_not_a_replay();
  test_sequence_wrap_does_not_deafen_a_node();
  test_reordering_inside_the_window();
  test_the_frame_that_moved_the_window_cannot_return();
  test_a_jump_past_the_window_forgets_only_what_it_must();
  test_eviction_is_least_recently_used();
  BOOMLINK_TEST_REPORT("dupcache_test");
}
