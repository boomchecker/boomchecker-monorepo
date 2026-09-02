/**
 ******************************************************************************
 * @file    system_service_test.c
 * @brief   Tests for boomlink_system_service_handle()/_arm_wakeup()/_poll()
 *          (boomlink.md sections 8.5-8.6, services/boomlink_system_service.h).
 *
 *          The guarantees under test: Ping always echoes Pong immediately;
 *          a WakeupRequest never answers synchronously and instead stages a
 *          delay that arm_wakeup() turns into a deadline poll() only fires
 *          once, using caller-supplied now_ms/random_u32 throughout, never
 *          this file's own; a second WakeupRequest restarts the window
 *          rather than queuing or being ignored; and an incoming
 *          WakeupResponse is reported through the injected ops callback,
 *          not built into a reply.
 ******************************************************************************
 */
#include "boomlink_system_service.h"

#include <string.h>

#include "c_test.h"

BOOMLINK_TEST_STATE;

static const boomlink_system_identity_t kIdentity = {
  .device_type      = boomlink_DeviceType_DEVICE_TYPE_STMNODE,
  .fw_version_major = 1,
  .fw_version_minor = 2,
  .fw_version_patch = 3,
};

static boomlink_dispatch_rx_info_t rx_from(uint32_t source_id) {
  boomlink_dispatch_rx_info_t rx = {0};
  rx.source_id                   = source_id;
  return rx;
}

static void test_ping_echoes_pong_immediately(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);

  boomlink_SystemMessage req                    = {0};
  req.which_message                             = boomlink_SystemMessage_ping_tag;
  req.message.ping.payload.size                 = 3;
  req.message.ping.payload.bytes[0]             = 'h';
  req.message.ping.payload.bytes[1]             = 'i';
  req.message.ping.payload.bytes[2]             = '!';

  boomlink_dispatch_rx_info_t rx = rx_from(0x11u);
  boomlink_SystemMessage      resp = {0};
  bool ok = boomlink_system_service_handle(&svc, &rx, &req, &resp);

  REQUIRE(ok, "Ping must always produce a response");
  REQUIRE(resp.which_message == boomlink_SystemMessage_pong_tag, "Ping must answer Pong");
  CHECK(resp.message.pong.payload.size == 3, "Pong payload size must match the Ping's");
  CHECK(memcmp(resp.message.pong.payload.bytes, "hi!", 3) == 0,
        "Pong payload must echo the Ping's bytes verbatim");
}

static void test_ping_echoes_pong_at_boundary_sizes(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);
  boomlink_dispatch_rx_info_t rx = rx_from(0x99u);

  /* size == 0: handle_ping()'s memcpy must tolerate a zero-length copy
     (memcpy(dst, src, 0) is well-defined, but worth pinning explicitly). */
  boomlink_SystemMessage req0             = {0};
  req0.which_message                      = boomlink_SystemMessage_ping_tag;
  req0.message.ping.payload.size          = 0;
  boomlink_SystemMessage resp0            = {0};
  REQUIRE(boomlink_system_service_handle(&svc, &rx, &req0, &resp0), "a zero-length Ping must still echo");
  CHECK(resp0.message.pong.payload.size == 0, "a zero-length Ping must produce a zero-length Pong");

  /* size == full capacity: the largest a decoded Ping can ever be
     (nanopb/system.options' own max_size) - the exact case handle_ping()'s
     _Static_assert (in the .c file, next to the memcpy) exists to keep
     memory-safe if Ping/Pong's capacities ever diverge. */
  boomlink_SystemMessage req_max          = {0};
  req_max.which_message                   = boomlink_SystemMessage_ping_tag;
  req_max.message.ping.payload.size       = sizeof(req_max.message.ping.payload.bytes);
  memset(req_max.message.ping.payload.bytes, 0xAB, sizeof(req_max.message.ping.payload.bytes));
  boomlink_SystemMessage resp_max         = {0};
  REQUIRE(boomlink_system_service_handle(&svc, &rx, &req_max, &resp_max), "a max-size Ping must still echo");
  CHECK(resp_max.message.pong.payload.size == sizeof(req_max.message.ping.payload.bytes),
        "a max-size Ping must produce a max-size Pong");
  CHECK(memcmp(resp_max.message.pong.payload.bytes, req_max.message.ping.payload.bytes,
               sizeof(req_max.message.ping.payload.bytes)) == 0,
        "a max-size Ping's bytes must be echoed verbatim");
}

static void test_wakeup_request_never_answers_synchronously(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);

  boomlink_SystemMessage req                = {0};
  req.which_message                         = boomlink_SystemMessage_wakeup_request_tag;
  req.message.wakeup_request.window_s       = 30u;

  boomlink_dispatch_rx_info_t rx = rx_from(0xAAu);
  boomlink_SystemMessage      resp = {0};
  bool ok = boomlink_system_service_handle(&svc, &rx, &req, &resp);

  CHECK(!ok, "a WakeupRequest must never answer synchronously - section 8.6's whole point");
  CHECK(svc.wakeup_staged, "the request must be staged for arm_wakeup() to pick up");
  CHECK(svc.staged_window_s == 30u, "the staged window_s must match the request");
  CHECK(svc.staged_reply_to == 0xAAu, "the staged reply-to address must be the requester's source_id");
  CHECK(!svc.wakeup_armed, "nothing is armed until arm_wakeup() runs");
}

static void test_arm_and_poll_full_cycle(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);

  boomlink_SystemMessage req          = {0};
  req.which_message                   = boomlink_SystemMessage_wakeup_request_tag;
  req.message.wakeup_request.window_s = 10u; /* window_ms = 10001 */
  boomlink_dispatch_rx_info_t rx      = rx_from(0x22u);
  boomlink_SystemMessage      resp    = {0};
  REQUIRE(!boomlink_system_service_handle(&svc, &rx, &req, &resp), "setup: stage the request");

  boomlink_system_service_arm_wakeup(&svc, 1000u, 4321u);
  REQUIRE(svc.wakeup_armed, "arm_wakeup() must arm after a staged request");
  CHECK(!svc.wakeup_staged, "arm_wakeup() must consume (clear) the staged request");
  uint32_t expected_delay = 4321u % 10001u;
  CHECK(svc.wakeup_armed_at_ms == 1000u, "wakeup_armed_at_ms must be the now_ms arm_wakeup() was called with");
  CHECK(svc.wakeup_delay_ms == expected_delay,
        "wakeup_delay_ms must be random_u32 mod (window_s*1000+1)");
  uint32_t fire_at_ms = svc.wakeup_armed_at_ms + svc.wakeup_delay_ms;

  boomlink_SystemMessage out_msg   = {0};
  uint32_t               reply_to  = 0;
  CHECK(!boomlink_system_service_poll(&svc, fire_at_ms - 1u, &out_msg, &reply_to),
        "poll() must not fire one millisecond before the deadline");

  bool fired = boomlink_system_service_poll(&svc, fire_at_ms, &out_msg, &reply_to);
  REQUIRE(fired, "poll() must fire exactly at the deadline");
  CHECK(out_msg.which_message == boomlink_SystemMessage_wakeup_response_tag,
        "poll() must fill a WakeupResponse");
  const boomlink_WakeupResponse *wr = &out_msg.message.wakeup_response;
  CHECK(wr->node_id == 0u,
        "node_id is deliberately left 0 - this service has no link address of its own, "
        "the caller must fill it in");
  CHECK(wr->device_type == boomlink_DeviceType_DEVICE_TYPE_STMNODE,
        "device_type must come from the identity this service was initialized with");
  CHECK(wr->fw_version_major == 1 && wr->fw_version_minor == 2 && wr->fw_version_patch == 3,
        "fw_version must come from the identity this service was initialized with");
  CHECK(reply_to == 0x22u, "out_reply_to must be the original requester's source_id");

  /* poll() cleared wakeup_armed on the fire above, so any later now_ms -
     even one far in the future - must not fire a second time for the same
     wakeup. */
  boomlink_SystemMessage out_msg2  = {0};
  uint32_t               reply_to2 = 0;
  CHECK(!boomlink_system_service_poll(&svc, fire_at_ms + 60000u, &out_msg2, &reply_to2),
        "poll() must not fire a second time for the same wakeup");
}

static void test_window_zero_fires_immediately(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);

  boomlink_SystemMessage req          = {0};
  req.which_message                   = boomlink_SystemMessage_wakeup_request_tag;
  req.message.wakeup_request.window_s = 0u;
  boomlink_dispatch_rx_info_t rx      = rx_from(0x33u);
  boomlink_SystemMessage      resp    = {0};
  REQUIRE(!boomlink_system_service_handle(&svc, &rx, &req, &resp), "setup: stage the request");

  /* window_s == 0 means window_ms == 1 (see arm_wakeup()'s own "+1" comment),
     so delay must be 0 regardless of the random draw - the only value
     `x % 1` can ever produce. */
  boomlink_system_service_arm_wakeup(&svc, 5000u, 0xFFFFFFFFu);
  CHECK(svc.wakeup_delay_ms == 0u,
        "window_s == 0 must draw a zero delay, whatever the random draw was");

  boomlink_SystemMessage out_msg = {0};
  uint32_t               reply_to = 0;
  CHECK(boomlink_system_service_poll(&svc, 5000u, &out_msg, &reply_to),
        "poll() must fire immediately at now_ms == fire_at_ms for a zero-delay wakeup");
}

static void test_arm_wakeup_clamps_oversize_window_s(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);

  /* window_s taken verbatim off an unauthenticated WakeupRequest frame can
     be any uint32_t. 4,294,968 is the smallest window_s whose UNCLAMPED
     `window_s * 1000u + 1u` overflows a 32-bit multiply (review's own
     finding): unclamped, that arithmetic wraps to window_ms == 705, and
     random_u32 == 0xFFFFFFFF against that wrapped window_ms draws
     delay_ms == 135 - an arbitrary, wrong, much-too-short window nothing
     about "clamp to BOOMLINK_SYSTEM_WAKEUP_MAX_WINDOW_S" would produce.
     Clamped (this file's actual behaviour), window_s becomes 3600,
     window_ms becomes 3,600,001, and 0xFFFFFFFF mod 3,600,001 is exactly
     166,102 - computed by hand, not merely range-checked, so this test
     fails loudly (166102 != 135) rather than passing by coincidence if the
     clamp were ever removed. */
  boomlink_SystemMessage req          = {0};
  req.which_message                   = boomlink_SystemMessage_wakeup_request_tag;
  req.message.wakeup_request.window_s = 4294968u;
  boomlink_dispatch_rx_info_t rx      = rx_from(0x40u);
  boomlink_SystemMessage      resp    = {0};
  REQUIRE(!boomlink_system_service_handle(&svc, &rx, &req, &resp), "setup: stage the oversize request");

  boomlink_system_service_arm_wakeup(&svc, 1000u, 0xFFFFFFFFu);
  REQUIRE(svc.wakeup_armed, "an oversize window_s must still arm, just clamped");
  CHECK(svc.wakeup_delay_ms == 166102u,
        "the drawn delay must come from the CLAMPED window (3600s -> 3,600,001ms), not from "
        "an unclamped multiply wrapping to an arbitrary, much-too-short window");

  /* A zero random draw against the clamped window must still deterministically
     draw a zero delay - proof the clamp did not introduce a stray
     modulo-by-zero for the largest possible staged_window_s. */
  boomlink_system_service_t svc2;
  boomlink_system_service_init(&svc2, &kIdentity, NULL);
  boomlink_system_service_handle(&svc2, &rx, &req, &resp);
  boomlink_system_service_arm_wakeup(&svc2, 1000u, 0u);
  CHECK(svc2.wakeup_delay_ms == 0u, "a zero random draw must still draw a zero delay after clamping");
}

static void test_poll_wraps_safely_across_tick_rollover(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);

  /* Arm right before HAL_GetTick() would wrap from UINT32_MAX back to 0,
     with a delay that pushes the real deadline past that wrap point -
     0xFFFFFFF0 + 32 = 0x100000010, which truncates to tick value 16 once
     wrapped. This is the exact scenario boomlink_config_service.c's
     window_elapsed() idiom (which this function's own comment now credits
     correctly) is designed to survive. */
  boomlink_SystemMessage req          = {0};
  req.which_message                   = boomlink_SystemMessage_wakeup_request_tag;
  req.message.wakeup_request.window_s = 100u; /* window_ms = 100001, comfortably > 32 */
  boomlink_dispatch_rx_info_t rx      = rx_from(0x50u);
  boomlink_SystemMessage      resp    = {0};
  boomlink_system_service_handle(&svc, &rx, &req, &resp);
  boomlink_system_service_arm_wakeup(&svc, 0xFFFFFFF0u, 32u);
  REQUIRE(svc.wakeup_delay_ms == 32u, "setup: the chosen random draw (32) is under window_ms, so delay == 32");

  boomlink_SystemMessage out_msg  = {0};
  uint32_t               reply_to = 0;
  CHECK(!boomlink_system_service_poll(&svc, 15u, &out_msg, &reply_to),
        "must not fire one tick before the wrapped deadline (tick 15, one before the wrapped tick 16)");
  CHECK(boomlink_system_service_poll(&svc, 16u, &out_msg, &reply_to),
        "must fire exactly at the wrapped deadline (tick 16), even though now_ms is numerically "
        "far smaller than wakeup_armed_at_ms");
}

static void test_wakeup_request_with_null_rx_stages_reply_to_zero(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);

  boomlink_SystemMessage req          = {0};
  req.which_message                   = boomlink_SystemMessage_wakeup_request_tag;
  req.message.wakeup_request.window_s = 10u;
  boomlink_SystemMessage resp         = {0};
  CHECK(!boomlink_system_service_handle(&svc, NULL, &req, &resp),
        "a WakeupRequest with no rx info must still stage, just answering false, same as with rx");
  CHECK(svc.staged_reply_to == 0u,
        "a NULL rx must stage reply_to as 0 rather than dereferencing a NULL source_id");
}

static void test_second_request_restarts_and_new_deadline_actually_fires(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);

  boomlink_SystemMessage req1          = {0};
  req1.which_message                   = boomlink_SystemMessage_wakeup_request_tag;
  req1.message.wakeup_request.window_s = 10u; /* window_ms = 10001 */
  boomlink_dispatch_rx_info_t rx1      = rx_from(0x01u);
  boomlink_SystemMessage      resp1    = {0};
  boomlink_system_service_handle(&svc, &rx1, &req1, &resp1);
  boomlink_system_service_arm_wakeup(&svc, 1000u, 4000u); /* would fire at tick 5000, if it ever got to */
  REQUIRE(svc.wakeup_armed, "setup: first request armed");

  boomlink_SystemMessage req2          = {0};
  req2.which_message                   = boomlink_SystemMessage_wakeup_request_tag;
  req2.message.wakeup_request.window_s = 10u;
  boomlink_dispatch_rx_info_t rx2      = rx_from(0x02u);
  boomlink_SystemMessage      resp2    = {0};
  boomlink_system_service_handle(&svc, &rx2, &req2, &resp2);
  REQUIRE(!svc.wakeup_armed, "setup: the second request's restart clears the old arm");
  boomlink_system_service_arm_wakeup(&svc, 2000u, 100u); /* fires much earlier, at tick 2100 */
  REQUIRE(svc.wakeup_armed, "setup: second request armed");

  boomlink_SystemMessage out_msg  = {0};
  uint32_t               reply_to = 0;
  REQUIRE(boomlink_system_service_poll(&svc, 2100u, &out_msg, &reply_to),
          "the SECOND request's own deadline must actually fire on schedule, not just look armed");
  CHECK(reply_to == 0x02u, "the fired response must go to the SECOND requester, not the first");

  /* poll() above cleared wakeup_armed. There is only ever one armed deadline
     in this struct - the restart already overwrote the first request's
     armed_at_ms/delay_ms before this point, so there is no "old deadline"
     left anywhere to fire a second time, stale or otherwise. */
  boomlink_SystemMessage out_msg2  = {0};
  uint32_t               reply_to2 = 0;
  CHECK(!boomlink_system_service_poll(&svc, 5000u, &out_msg2, &reply_to2),
        "no second fire for the same wakeup, and no trace of the superseded first request's deadline");
}

static void test_second_request_restarts_the_window_while_staged(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);

  boomlink_SystemMessage req1          = {0};
  req1.which_message                   = boomlink_SystemMessage_wakeup_request_tag;
  req1.message.wakeup_request.window_s = 5u;
  boomlink_dispatch_rx_info_t rx1      = rx_from(0x01u);
  boomlink_SystemMessage      resp1    = {0};
  REQUIRE(!boomlink_system_service_handle(&svc, &rx1, &req1, &resp1), "setup: stage the first request");

  boomlink_SystemMessage req2          = {0};
  req2.which_message                   = boomlink_SystemMessage_wakeup_request_tag;
  req2.message.wakeup_request.window_s = 60u;
  boomlink_dispatch_rx_info_t rx2      = rx_from(0x02u);
  boomlink_SystemMessage      resp2    = {0};
  CHECK(!boomlink_system_service_handle(&svc, &rx2, &req2, &resp2),
        "a second WakeupRequest must still never answer synchronously");

  CHECK(svc.staged_window_s == 60u, "the SECOND request's window_s must win, not the first's");
  CHECK(svc.staged_reply_to == 0x02u, "the SECOND request's reply-to address must win, not the first's");
}

static void test_second_request_restarts_an_already_armed_window(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);

  boomlink_SystemMessage req1          = {0};
  req1.which_message                   = boomlink_SystemMessage_wakeup_request_tag;
  req1.message.wakeup_request.window_s = 5u;
  boomlink_dispatch_rx_info_t rx1      = rx_from(0x01u);
  boomlink_SystemMessage      resp1    = {0};
  boomlink_system_service_handle(&svc, &rx1, &req1, &resp1);
  boomlink_system_service_arm_wakeup(&svc, 1000u, 2000u); /* now armed */
  REQUIRE(svc.wakeup_armed, "setup: first request is armed");

  boomlink_SystemMessage req2          = {0};
  req2.which_message                   = boomlink_SystemMessage_wakeup_request_tag;
  req2.message.wakeup_request.window_s = 5u;
  boomlink_dispatch_rx_info_t rx2      = rx_from(0x02u);
  boomlink_SystemMessage      resp2    = {0};
  boomlink_system_service_handle(&svc, &rx2, &req2, &resp2);

  CHECK(!svc.wakeup_armed,
        "a new WakeupRequest arriving while one is already ARMED must clear the old arm, "
        "not race it - section 8.6's restart rule applies here too, not just while staged");
  CHECK(svc.wakeup_staged, "the new request must be staged, ready for its own arm_wakeup()");
  CHECK(svc.staged_reply_to == 0x02u, "the new request's reply-to address must be staged");
}

typedef struct {
  int      calls;
  uint32_t last_source_id;
  boomlink_WakeupResponse last_response;
} response_spy_t;

static void on_wakeup_response_spy(void *ctx, uint32_t source_id, const boomlink_WakeupResponse *response) {
  response_spy_t *spy   = (response_spy_t *)ctx;
  spy->calls++;
  spy->last_source_id  = source_id;
  spy->last_response   = *response;
}

static void test_incoming_wakeup_response_calls_ops_and_answers_nothing(void) {
  response_spy_t spy = {0};
  boomlink_system_ops_t ops = {.on_wakeup_response = on_wakeup_response_spy, .ctx = &spy};
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, &ops);

  boomlink_SystemMessage req                                    = {0};
  req.which_message                                             = boomlink_SystemMessage_wakeup_response_tag;
  req.message.wakeup_response.node_id                           = 0x55u;
  req.message.wakeup_response.device_type                       = boomlink_DeviceType_DEVICE_TYPE_STMNODE;
  req.message.wakeup_response.fw_version_major                  = 9u;
  boomlink_dispatch_rx_info_t rx                                = rx_from(0x55u);
  boomlink_SystemMessage      resp                               = {0};
  bool ok = boomlink_system_service_handle(&svc, &rx, &req, &resp);

  CHECK(!ok, "a response to a response has nothing to correlate against and nobody to answer");
  REQUIRE(spy.calls == 1, "on_wakeup_response must be called exactly once");
  CHECK(spy.last_source_id == 0x55u, "the reported source_id must be the link frame's own source_id");
  CHECK(spy.last_response.node_id == 0x55u && spy.last_response.fw_version_major == 9u,
        "the reported WakeupResponse content must match what was received");
}

static void test_wakeup_request_does_not_invoke_response_ops(void) {
  response_spy_t spy = {0};
  boomlink_system_ops_t ops = {.on_wakeup_response = on_wakeup_response_spy, .ctx = &spy};
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, &ops);

  boomlink_SystemMessage req          = {0};
  req.which_message                   = boomlink_SystemMessage_wakeup_request_tag;
  req.message.wakeup_request.window_s = 5u;
  boomlink_dispatch_rx_info_t rx      = rx_from(0x44u);
  boomlink_SystemMessage      resp    = {0};
  boomlink_system_service_handle(&svc, &rx, &req, &resp);

  CHECK(spy.calls == 0,
        "on_wakeup_response must only fire for an incoming WakeupResponse, never a WakeupRequest");
}

static void test_incoming_wakeup_response_with_null_ops_does_not_crash(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL); /* NULL ops - see the struct's own doc */

  boomlink_SystemMessage req = {0};
  req.which_message          = boomlink_SystemMessage_wakeup_response_tag;
  boomlink_dispatch_rx_info_t rx   = rx_from(0x66u);
  boomlink_SystemMessage      resp = {0};
  CHECK(!boomlink_system_service_handle(&svc, &rx, &req, &resp),
        "a NULL ops must not crash and must still answer false");
}

static void test_unrecognized_message_answers_nothing(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);

  boomlink_SystemMessage req      = {0};
  req.which_message               = 0xFFu; /* not one of the four real tags */
  boomlink_dispatch_rx_info_t rx  = rx_from(0x77u);
  boomlink_SystemMessage      resp = {0};
  CHECK(!boomlink_system_service_handle(&svc, &rx, &req, &resp),
        "an unrecognized which_message must answer false, not crash");
}

static void test_handle_is_null_tolerant(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);
  boomlink_SystemMessage req  = {0};
  req.which_message           = boomlink_SystemMessage_ping_tag;
  boomlink_SystemMessage resp = {0};
  boomlink_dispatch_rx_info_t rx = rx_from(0x88u);

  CHECK(!boomlink_system_service_handle(NULL, &rx, &req, &resp), "NULL svc must be tolerated");
  CHECK(!boomlink_system_service_handle(&svc, &rx, NULL, &resp), "NULL request must be tolerated");
  CHECK(!boomlink_system_service_handle(&svc, &rx, &req, NULL), "NULL out_response must be tolerated");
}

static void test_arm_and_poll_are_null_and_no_op_tolerant(void) {
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, &kIdentity, NULL);

  /* Nothing staged - must be a safe no-op, not a crash or a spurious arm. */
  boomlink_system_service_arm_wakeup(&svc, 1000u, 42u);
  CHECK(!svc.wakeup_armed, "arm_wakeup() with nothing staged must not arm anything");
  boomlink_system_service_arm_wakeup(NULL, 1000u, 42u); /* must not crash */

  boomlink_SystemMessage out_msg = {0};
  uint32_t               reply_to = 0;
  CHECK(!boomlink_system_service_poll(&svc, 1000u, &out_msg, &reply_to),
        "poll() with nothing armed must return false");
  CHECK(!boomlink_system_service_poll(NULL, 1000u, &out_msg, &reply_to), "NULL svc must be tolerated");
  CHECK(!boomlink_system_service_poll(&svc, 1000u, NULL, &reply_to), "NULL out_message must be tolerated");
  CHECK(!boomlink_system_service_poll(&svc, 1000u, &out_msg, NULL), "NULL out_reply_to must be tolerated");
}

static void test_init_is_null_tolerant(void) {
  boomlink_system_service_init(NULL, &kIdentity, NULL); /* must not crash */
  boomlink_system_service_t svc;
  boomlink_system_service_init(&svc, NULL, NULL);
  CHECK(svc.identity.device_type == boomlink_DeviceType_DEVICE_TYPE_UNSPECIFIED,
        "a NULL identity must leave it zero-initialized, not garbage");
}

int main(void) {
  test_ping_echoes_pong_immediately();
  test_ping_echoes_pong_at_boundary_sizes();
  test_wakeup_request_never_answers_synchronously();
  test_arm_and_poll_full_cycle();
  test_window_zero_fires_immediately();
  test_arm_wakeup_clamps_oversize_window_s();
  test_poll_wraps_safely_across_tick_rollover();
  test_wakeup_request_with_null_rx_stages_reply_to_zero();
  test_second_request_restarts_and_new_deadline_actually_fires();
  test_second_request_restarts_the_window_while_staged();
  test_second_request_restarts_an_already_armed_window();
  test_incoming_wakeup_response_calls_ops_and_answers_nothing();
  test_wakeup_request_does_not_invoke_response_ops();
  test_incoming_wakeup_response_with_null_ops_does_not_crash();
  test_unrecognized_message_answers_nothing();
  test_handle_is_null_tolerant();
  test_arm_and_poll_are_null_and_no_op_tolerant();
  test_init_is_null_tolerant();
  BOOMLINK_TEST_REPORT("system_service_test", 69); /* exact count from running the compiled binary */
}
