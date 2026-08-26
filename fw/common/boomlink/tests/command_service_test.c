/**
 ******************************************************************************
 * @file    command_service_test.c
 * @brief   Tests for boomlink_command_service_handle() (boomlink.md section
 *          8.3, services/boomlink_command_service.h).
 *
 *          The guarantee under test: every CommandType maps to exactly one
 *          injected ops callback (or COMMAND_RESULT_UNSUPPORTED if that slot
 *          is NULL), the callback's own bool return becomes OK/FAILED, and a
 *          response is always built for a real CommandRequest - section
 *          8.3's "commands... return a correlated response" is not
 *          conditional on the command being recognized.
 ******************************************************************************
 */
#include "boomlink_command_service.h"

#include <stdio.h>
#include <string.h>

#include "c_test.h"

BOOMLINK_TEST_STATE;

/* One spy per command slot: separate globals rather than routing all seven
   through ops.ctx, since ops.ctx is a single shared pointer and this file
   wants independent control-and-inspection per command. */
typedef struct {
  int  calls;
  bool succeed;
  const char *diagnostic_to_write; /* NULL = the callback writes nothing */
  bool fill_capacity_with_non_nul; /* simulate a callback that ignores the NUL convention */
} spy_t;

static spy_t g_reboot, g_identify, g_self_test, g_start_detection, g_stop_detection,
    g_clear_statistics, g_request_diagnostics;

static void reset_spies(void) {
  g_reboot = g_identify = g_self_test = g_start_detection = g_stop_detection =
      g_clear_statistics = g_request_diagnostics = (spy_t){0};
}

static bool simple_cb(spy_t *spy) {
  spy->calls++;
  return spy->succeed;
}

static bool reboot_cb(void *ctx) { (void)ctx; return simple_cb(&g_reboot); }
static bool identify_cb(void *ctx) { (void)ctx; return simple_cb(&g_identify); }
static bool start_detection_cb(void *ctx) { (void)ctx; return simple_cb(&g_start_detection); }
static bool stop_detection_cb(void *ctx) { (void)ctx; return simple_cb(&g_stop_detection); }
static bool clear_statistics_cb(void *ctx) { (void)ctx; return simple_cb(&g_clear_statistics); }

static bool diagnostic_cb(spy_t *spy, char *out, size_t cap) {
  spy->calls++;
  if (spy->fill_capacity_with_non_nul) {
    /* A plausible but wrong implementation: uses every byte it was handed,
       leaving no room of its own for a terminator. The service must still
       hand back something Nanopb can encode. */
    memset(out, 'A', cap);
  } else if (spy->diagnostic_to_write != NULL) {
    snprintf(out, cap, "%s", spy->diagnostic_to_write);
  }
  return spy->succeed;
}
static bool self_test_cb(void *ctx, char *out, size_t cap) {
  (void)ctx;
  return diagnostic_cb(&g_self_test, out, cap);
}
static bool request_diagnostics_cb(void *ctx, char *out, size_t cap) {
  (void)ctx;
  return diagnostic_cb(&g_request_diagnostics, out, cap);
}

static boomlink_command_ops_t full_ops(void) {
  boomlink_command_ops_t ops = {0};
  ops.reboot              = reboot_cb;
  ops.identify            = identify_cb;
  ops.self_test           = self_test_cb;
  ops.start_detection     = start_detection_cb;
  ops.stop_detection      = stop_detection_cb;
  ops.clear_statistics    = clear_statistics_cb;
  ops.request_diagnostics = request_diagnostics_cb;
  return ops;
}

static bool run(const boomlink_command_ops_t *ops, boomlink_CommandType type,
               boomlink_CommandMessage *out_response) {
  boomlink_CommandMessage request = {0};
  request.which_message           = boomlink_CommandMessage_request_tag;
  request.message.request.type    = type;
  boomlink_dispatch_rx_info_t rx  = {0};
  return boomlink_command_service_handle((void *)ops, &rx, &request, out_response);
}

typedef struct {
  boomlink_CommandType type;
  spy_t                *spy;
  const char            *name;
} command_slot_t;

/* Addresses of file-scope globals are constant expressions, so this table
   can be a plain static initializer - no per-call assembly required. */
static const command_slot_t SLOTS[] = {
    {boomlink_CommandType_COMMAND_TYPE_REBOOT, &g_reboot, "reboot"},
    {boomlink_CommandType_COMMAND_TYPE_IDENTIFY, &g_identify, "identify"},
    {boomlink_CommandType_COMMAND_TYPE_SELF_TEST, &g_self_test, "self_test"},
    {boomlink_CommandType_COMMAND_TYPE_START_DETECTION, &g_start_detection, "start_detection"},
    {boomlink_CommandType_COMMAND_TYPE_STOP_DETECTION, &g_stop_detection, "stop_detection"},
    {boomlink_CommandType_COMMAND_TYPE_CLEAR_STATISTICS, &g_clear_statistics, "clear_statistics"},
    {boomlink_CommandType_COMMAND_TYPE_REQUEST_DIAGNOSTICS, &g_request_diagnostics,
     "request_diagnostics"},
};
#define SLOT_COUNT (sizeof(SLOTS) / sizeof(SLOTS[0]))

static void test_every_wired_command_maps_ok_or_failed_to_its_own_callback(void) {
  boomlink_command_ops_t ops = full_ops();

  for (size_t i = 0; i < SLOT_COUNT; i++) {
    command_slot_t slot = SLOTS[i];

    reset_spies();
    slot.spy->succeed = true;
    boomlink_CommandMessage resp;
    bool ok = run(&ops, slot.type, &resp);
    CHECK(ok, "%s: a real CommandRequest must always get a response", slot.name);
    CHECK(resp.which_message == boomlink_CommandMessage_response_tag,
          "%s: must answer with a CommandResponse", slot.name);
    CHECK(resp.message.response.type == slot.type, "%s: must echo the requested type", slot.name);
    CHECK(resp.message.response.result == boomlink_CommandResult_COMMAND_RESULT_OK,
          "%s: a true callback must map to OK", slot.name);
    CHECK(slot.spy->calls == 1, "%s: its own callback must be invoked exactly once, got %d",
          slot.name, slot.spy->calls);

    reset_spies();
    slot.spy->succeed = false;
    ok = run(&ops, slot.type, &resp);
    CHECK(ok, "%s: a response is still built when the callback fails", slot.name);
    CHECK(resp.message.response.result == boomlink_CommandResult_COMMAND_RESULT_FAILED,
          "%s: a false callback must map to FAILED", slot.name);
  }
}

static void test_every_unwired_command_is_unsupported(void) {
  boomlink_command_ops_t ops = {0}; /* every callback NULL */

  for (size_t i = 0; i < SLOT_COUNT; i++) {
    command_slot_t slot = SLOTS[i];
    boomlink_CommandMessage resp;
    bool ok = run(&ops, slot.type, &resp);
    CHECK(ok, "%s: a response is still built even with no ops wired", slot.name);
    CHECK(resp.message.response.result == boomlink_CommandResult_COMMAND_RESULT_UNSUPPORTED,
          "%s: a NULL callback slot must answer UNSUPPORTED, not crash", slot.name);
    CHECK(strlen(resp.message.response.diagnostic) == 0u,
          "%s: an unsupported command must not carry a stray diagnostic string", slot.name);
  }
}

static void test_unspecified_type_is_unsupported_even_with_everything_wired(void) {
  reset_spies();
  boomlink_command_ops_t ops = full_ops();
  boomlink_CommandMessage resp;
  bool ok = run(&ops, boomlink_CommandType_COMMAND_TYPE_UNSPECIFIED, &resp);

  CHECK(ok, "a response is still built for the unspecified type");
  CHECK(resp.message.response.result == boomlink_CommandResult_COMMAND_RESULT_UNSUPPORTED,
        "proto3's zero value must fall through to UNSUPPORTED, not call any wired ops slot");
  CHECK(g_reboot.calls == 0 && g_identify.calls == 0 && g_self_test.calls == 0 &&
            g_start_detection.calls == 0 && g_stop_detection.calls == 0 &&
            g_clear_statistics.calls == 0 && g_request_diagnostics.calls == 0,
        "no ops callback must be invoked for an unrecognized type");
}

static void test_diagnostic_commands_fill_the_diagnostic_string(void) {
  reset_spies();
  boomlink_command_ops_t ops = full_ops();
  g_self_test.succeed             = true;
  g_self_test.diagnostic_to_write = "self-test: battery ok";
  g_request_diagnostics.succeed             = true;
  g_request_diagnostics.diagnostic_to_write = "uptime 42s";

  boomlink_CommandMessage resp;
  run(&ops, boomlink_CommandType_COMMAND_TYPE_SELF_TEST, &resp);
  CHECK(strcmp(resp.message.response.diagnostic, "self-test: battery ok") == 0,
        "self_test's diagnostic text must reach the response, got \"%s\"",
        resp.message.response.diagnostic);

  run(&ops, boomlink_CommandType_COMMAND_TYPE_REQUEST_DIAGNOSTICS, &resp);
  CHECK(strcmp(resp.message.response.diagnostic, "uptime 42s") == 0,
        "request_diagnostics' text must reach the response, got \"%s\"",
        resp.message.response.diagnostic);

  /* A non-diagnostic command must never populate the field, even when its
     own callback succeeds - run_simple() never touches resp->diagnostic. */
  run(&ops, boomlink_CommandType_COMMAND_TYPE_IDENTIFY, &resp);
  CHECK(strlen(resp.message.response.diagnostic) == 0u,
        "a non-diagnostic command's response must have an empty diagnostic field");
}

static void test_diagnostic_buffer_is_zeroed_when_the_callback_writes_nothing(void) {
  reset_spies();
  boomlink_command_ops_t ops = full_ops();
  g_self_test.succeed             = true;
  g_self_test.diagnostic_to_write = NULL; /* callback runs but writes nothing */

  boomlink_CommandMessage resp;
  run(&ops, boomlink_CommandType_COMMAND_TYPE_SELF_TEST, &resp);
  CHECK(strlen(resp.message.response.diagnostic) == 0u,
        "the diagnostic buffer must start zeroed, not carry stale bytes when unwritten");
}

static void test_diagnostic_buffer_is_always_nul_terminated_even_if_the_callback_fills_it(void) {
  reset_spies();
  boomlink_command_ops_t ops = full_ops();
  g_self_test.succeed                    = true;
  g_self_test.fill_capacity_with_non_nul = true;

  boomlink_CommandMessage resp;
  run(&ops, boomlink_CommandType_COMMAND_TYPE_SELF_TEST, &resp);

  size_t cap = sizeof(resp.message.response.diagnostic);
  REQUIRE(resp.message.response.diagnostic[cap - 1] == '\0',
          "the last byte of the diagnostic buffer must always be NUL, regardless of what a "
          "callback wrote into it - Nanopb's string encoder requires this to encode at all, "
          "and the strlen() below is undefined behavior on an unterminated buffer, so this "
          "must stop the scenario rather than continue past a real regression here");
  size_t len = strlen(resp.message.response.diagnostic);
  CHECK(len == cap - 1,
        "a callback that fills every byte it was handed must still leave a valid string, "
        "truncated to make room for the reserved terminator, got length %zu", len);
}

static void test_malformed_or_missing_arguments_are_rejected(void) {
  boomlink_command_ops_t ops = full_ops();
  boomlink_dispatch_rx_info_t rx = {0};
  boomlink_CommandMessage response;

  boomlink_CommandMessage valid_request = {0};
  valid_request.which_message        = boomlink_CommandMessage_request_tag;
  valid_request.message.request.type = boomlink_CommandType_COMMAND_TYPE_IDENTIFY;

  boomlink_CommandMessage not_a_request = {0};
  not_a_request.which_message = boomlink_CommandMessage_response_tag;
  CHECK(!boomlink_command_service_handle(&ops, &rx, &not_a_request, &response),
        "a decoded CommandResponse is not something this service can answer");

  CHECK(!boomlink_command_service_handle(NULL, &rx, &valid_request, &response),
        "a NULL ops pointer must be rejected, not dereferenced");
  CHECK(!boomlink_command_service_handle(&ops, &rx, NULL, &response),
        "a NULL request must be rejected");
  CHECK(!boomlink_command_service_handle(&ops, &rx, &valid_request, NULL),
        "a NULL out_response must be rejected");
}

/* Section 9.9: "commands that are dangerous when broadcast should be
   rejected by the application service unless explicitly designed for
   broadcast." Reboot is the case this names, and this service is the only
   point in the dispatch chain that ever sees both `rx->destination_id` and
   the command type together (boomlink_command_ops_t's per-command
   callbacks do not receive `rx` at all - see that struct's own doc). */
static void test_reboot_over_broadcast_is_rejected_without_calling_reboot(void) {
  reset_spies();
  boomlink_command_ops_t ops = full_ops();
  g_reboot.succeed = true; /* would answer OK if called - proves it was not */

  boomlink_CommandMessage request = {0};
  request.which_message        = boomlink_CommandMessage_request_tag;
  request.message.request.type = boomlink_CommandType_COMMAND_TYPE_REBOOT;

  boomlink_dispatch_rx_info_t rx = {0};
  /* BOOMLINK_ADDR_BROADCAST (section 7.2) spelled out as a literal rather
     than included from boomlink_linkframe.h - this test target links
     boomlink_command_service only, which pulls that header in PRIVATEly
     (see fw/common/boomlink/CMakeLists.txt's comment on that dependency),
     the same reason config_service_test.c spells out its own magic-default
     literal instead of including it. */
  rx.destination_id = 0xFFFFFFFFu;

  boomlink_CommandMessage resp;
  bool ok = boomlink_command_service_handle(&ops, &rx, &request, &resp);

  CHECK(ok, "a response is still built for a rejected broadcast reboot");
  CHECK(resp.message.response.result == boomlink_CommandResult_COMMAND_RESULT_FAILED,
        "a broadcast Reboot must be refused with FAILED, not UNSUPPORTED - this node CAN "
        "reboot, it is this specific broadcast request that is refused");
  CHECK(g_reboot.calls == 0,
        "ops->reboot must never be invoked for a broadcast Reboot request, got %d calls",
        g_reboot.calls);

  /* A targeted rejection of broadcast, not a regression in Reboot itself -
     the identical request addressed to a real unicast destination must
     still succeed normally. */
  reset_spies();
  g_reboot.succeed  = true;
  rx.destination_id = 42u;
  ok                = boomlink_command_service_handle(&ops, &rx, &request, &resp);
  CHECK(ok, "a unicast reboot request must still get a response");
  CHECK(resp.message.response.result == boomlink_CommandResult_COMMAND_RESULT_OK,
        "a unicast reboot must still succeed normally");
  CHECK(g_reboot.calls == 1, "a unicast reboot must still call ops->reboot exactly once, got %d",
        g_reboot.calls);
}

int main(void) {
  test_every_wired_command_maps_ok_or_failed_to_its_own_callback();
  test_every_unwired_command_is_unsupported();
  test_unspecified_type_is_unsupported_even_with_everything_wired();
  test_diagnostic_commands_fill_the_diagnostic_string();
  test_diagnostic_buffer_is_zeroed_when_the_callback_writes_nothing();
  test_diagnostic_buffer_is_always_nul_terminated_even_if_the_callback_fills_it();
  test_malformed_or_missing_arguments_are_rejected();
  test_reboot_over_broadcast_is_rejected_without_calling_reboot();
  BOOMLINK_TEST_REPORT("command_service_test", 89);
}
