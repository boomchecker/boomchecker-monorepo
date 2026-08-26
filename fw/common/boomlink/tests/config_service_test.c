/**
 ******************************************************************************
 * @file    config_service_test.c
 * @brief   Tests for boomlink_config_service_handle() and the revert-on-
 *          timeout apply state machine (boomlink.md section 8.2,
 *          services/boomlink_config_service.h).
 *
 *          The guarantees under test: a GET only ever populates the groups
 *          it was asked for; a SET rejects a stale expected_config_version
 *          without touching state; each present group in a SET is a WHOLE-
 *          GROUP REPLACEMENT (config.proto's own documented contract - an
 *          unrestated scalar is wiped to its zero default, not left alone);
 *          node_id/magic/RadioConfig are hazardous and must not reach
 *          `current` before boomlink_config_service_commit_pending_apply()
 *          is called, and must revert if boomlink_config_service_poll()
 *          finds the confirmation window elapsed with no
 *          boomlink_config_service_confirm_pending_apply().
 ******************************************************************************
 */
#include "boomlink_config_service.h"

#include "c_test.h"

BOOMLINK_TEST_STATE;

static boomlink_config_service_t make_svc(uint32_t confirm_window_ms) {
  boomlink_config_service_t svc;
  boomlink_node_config_t    initial;
  boomlink_node_config_defaults(&initial);
  boomlink_config_service_init(&svc, &initial, confirm_window_ms);
  return svc;
}

static boomlink_ConfigMessage get_request(bool general, bool link, bool radio, bool detection,
                                          bool gnss, bool telemetry) {
  boomlink_ConfigMessage msg = {0};
  msg.which_message                        = boomlink_ConfigMessage_get_request_tag;
  msg.message.get_request.include_general   = general;
  msg.message.get_request.include_link      = link;
  msg.message.get_request.include_radio     = radio;
  msg.message.get_request.include_detection = detection;
  msg.message.get_request.include_gnss      = gnss;
  msg.message.get_request.include_telemetry = telemetry;
  return msg;
}

static bool handle(boomlink_config_service_t *svc, const boomlink_ConfigMessage *request,
                   boomlink_ConfigMessage *out_response) {
  boomlink_dispatch_rx_info_t rx = {0};
  return boomlink_config_service_handle(svc, &rx, request, out_response);
}

static void test_get_returns_only_requested_groups(void) {
  boomlink_config_service_t svc            = make_svc(1000u);
  svc.current.general.node_id              = 100u;
  svc.current.telemetry.report_interval_s  = 30u;
  svc.current.radio.frequency_mhz          = 868.1f;

  boomlink_ConfigMessage req = get_request(true, false, false, false, false, true);
  boomlink_ConfigMessage resp;
  bool ok = handle(&svc, &req, &resp);

  REQUIRE(ok, "a GET must always answer");
  CHECK(resp.which_message == boomlink_ConfigMessage_get_response_tag,
        "a GET answers with a get_response");
  boomlink_ConfigGetResponse *gr = &resp.message.get_response;
  CHECK(gr->config_version == 1u, "echoes the current version, got %u", (unsigned)gr->config_version);
  CHECK(gr->has_general && gr->general.node_id == 100u, "general was requested and populated");
  CHECK(gr->has_telemetry && gr->telemetry.report_interval_s == 30u,
        "telemetry was requested and populated");
  CHECK(!gr->has_link, "link was not requested");
  CHECK(!gr->has_radio, "radio was not requested, even though it holds a real non-zero value");
  CHECK(!gr->has_detection, "detection was not requested");
  CHECK(!gr->has_gnss, "gnss was not requested");
}

static void test_set_rejects_stale_expected_version(void) {
  boomlink_config_service_t svc = make_svc(1000u);

  boomlink_ConfigMessage req                              = {0};
  req.which_message                                       = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version         = 999u; /* current is 1 */
  req.message.set_request.has_telemetry                   = true;
  req.message.set_request.telemetry.report_interval_s     = 42u;

  boomlink_ConfigMessage resp;
  bool ok = handle(&svc, &req, &resp);

  REQUIRE(ok, "a SET always answers, even a rejected one");
  CHECK(resp.which_message == boomlink_ConfigMessage_set_response_tag,
        "a SET answers with a set_response");
  CHECK(resp.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_VERSION_CONFLICT,
        "a stale expected_config_version must be rejected");
  CHECK(resp.message.set_response.config_version == 1u,
        "the response reports the REAL current version, got %u",
        (unsigned)resp.message.set_response.config_version);
  CHECK(svc.current.telemetry.report_interval_s == 0u,
        "a rejected write must not touch state, got %u",
        (unsigned)svc.current.telemetry.report_interval_s);
  CHECK(svc.current.config_version == 1u, "a rejected write must not bump the version");
}

static void test_non_hazardous_set_applies_immediately_as_a_whole_group(void) {
  boomlink_config_service_t svc              = make_svc(1000u);
  svc.current.general.default_destination_id = 55u; /* the next SET will not restate this */
  svc.current.general.receive_enabled        = true;

  /* A fresh zero-initialized GeneralConfig other than node_id, which is left
     equal to the current value (0) - so this is NOT a hazardous change, and
     every other field is a real write of the zero default. */
  boomlink_ConfigMessage req                      = {0};
  req.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version = 1u;
  req.message.set_request.has_general             = true;

  boomlink_ConfigMessage resp;
  bool ok = handle(&svc, &req, &resp);

  REQUIRE(ok, "a SET always answers");
  CHECK(resp.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_OK,
        "no hazardous field changed, so this applies immediately as OK");
  CHECK(resp.message.set_response.config_version == 2u, "an applied SET bumps the version");
  CHECK(svc.current.config_version == 2u, "current must reflect the bump too");
  CHECK(svc.current.general.default_destination_id == 0u,
        "whole-group replacement: an unrestated field is wiped to its zero default, got %u",
        (unsigned)svc.current.general.default_destination_id);
  CHECK(!svc.current.general.receive_enabled,
        "whole-group replacement: receive_enabled must be wiped too, not left true");
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_IDLE, "no hazard changed, so nothing is pending");
}

static void test_set_leaves_omitted_groups_untouched(void) {
  boomlink_config_service_t svc          = make_svc(1000u);
  svc.current.general.default_destination_id = 55u;
  svc.current.link.ack_timeout_margin_ms     = 250u;

  boomlink_ConfigMessage req                          = {0};
  req.which_message                                   = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version     = 1u;
  req.message.set_request.has_telemetry               = true;
  req.message.set_request.telemetry.report_interval_s = 60u;
  /* has_general and has_link both left false: neither group is even
     mentioned in this write. */

  boomlink_ConfigMessage resp;
  handle(&svc, &req, &resp);

  CHECK(svc.current.general.default_destination_id == 55u,
        "a group not present in the SET must be left exactly as it was");
  CHECK(svc.current.link.ack_timeout_margin_ms == 250u, "same for a second untouched group");
  CHECK(svc.current.telemetry.report_interval_s == 60u, "the one requested group is updated");
}

static void test_hazardous_node_id_change_is_staged_but_sibling_fields_apply_now(void) {
  boomlink_config_service_t svc = make_svc(1000u);

  boomlink_ConfigMessage req                              = {0};
  req.which_message                                       = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version         = 1u;
  req.message.set_request.has_general                     = true;
  req.message.set_request.general.node_id                 = 77u; /* hazardous: differs from 0 */
  req.message.set_request.general.default_destination_id  = 5u;  /* not hazardous */

  boomlink_ConfigMessage resp;
  bool ok = handle(&svc, &req, &resp);

  REQUIRE(ok, "a SET always answers");
  CHECK(resp.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_PENDING_CONFIRMATION,
        "a node_id change must be staged, not applied outright");
  CHECK(resp.message.set_response.config_version == 2u, "the version still bumps for a staged change");
  CHECK(svc.current.general.node_id == 0u,
        "the hazardous field must NOT be in current yet, got %u",
        (unsigned)svc.current.general.node_id);
  CHECK(svc.current.general.default_destination_id == 5u,
        "a non-hazardous sibling field in the same group applies immediately regardless");
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_STAGED, "apply_state must be STAGED");
  CHECK(svc.staged.node_id == 77u, "the new node_id is held in staged");
  CHECK(svc.revert_to.node_id == 0u, "the pre-change node_id is held in revert_to");
}

static void test_hazardous_magic_change_is_staged(void) {
  boomlink_config_service_t svc = make_svc(1000u);

  boomlink_ConfigMessage req                            = {0};
  req.which_message                                     = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version       = 1u;
  req.message.set_request.has_link                      = true;
  req.message.set_request.link.magic                    = 0xABu; /* hazardous: differs from 0 */
  req.message.set_request.link.ack_timeout_margin_ms     = 300u;  /* not hazardous */

  boomlink_ConfigMessage resp;
  handle(&svc, &req, &resp);

  CHECK(resp.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_PENDING_CONFIRMATION,
        "a magic change must be staged");
  CHECK(svc.current.link.magic == 0u, "magic must not be in current yet");
  CHECK(svc.current.link.ack_timeout_margin_ms == 300u,
        "a non-hazardous sibling field applies immediately");
  CHECK(svc.staged.magic == 0xABu, "the new magic is held in staged");
}

static void test_hazardous_radio_change_touches_nothing_until_committed(void) {
  boomlink_config_service_t svc   = make_svc(1000u);
  svc.current.radio.frequency_mhz = 868.1f;

  boomlink_ConfigMessage req                          = {0};
  req.which_message                                   = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version     = 1u;
  req.message.set_request.has_radio                   = true;
  req.message.set_request.radio.frequency_mhz         = 915.0f;
  req.message.set_request.radio.spreading_factor      = 9u;

  boomlink_ConfigMessage resp;
  handle(&svc, &req, &resp);

  CHECK(resp.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_PENDING_CONFIRMATION,
        "any RadioConfig change is entirely hazardous");
  CHECK(svc.current.radio.frequency_mhz == 868.1f,
        "RadioConfig must be completely untouched until committed, not even sibling fields");
  CHECK(svc.current.radio.spreading_factor == 0u, "same for every other RadioConfig field");
  CHECK(svc.staged.radio.frequency_mhz == 915.0f, "the new profile is held in staged");
}

static void test_requesting_the_current_hazardous_value_is_not_a_hazard_change(void) {
  boomlink_config_service_t svc = make_svc(1000u);
  /* current.general.node_id is 0 by default - request the SAME value. */

  boomlink_ConfigMessage req                      = {0};
  req.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version = 1u;
  req.message.set_request.has_general             = true;
  req.message.set_request.general.node_id         = 0u;

  boomlink_ConfigMessage resp;
  handle(&svc, &req, &resp);

  CHECK(resp.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_OK,
        "restating the same node_id is not a hazard - must apply immediately as OK");
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_IDLE, "nothing to stage");
}

static void test_commit_pending_apply_requires_staged(void) {
  boomlink_config_service_t svc = make_svc(1000u);

  boomlink_config_service_commit_pending_apply(&svc, 500u);
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_IDLE, "commit with nothing staged must be a no-op");
  CHECK(svc.apply_started_at_ms == 0u, "and must not touch apply_started_at_ms");

  boomlink_ConfigMessage req                      = {0};
  req.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version = 1u;
  req.message.set_request.has_general             = true;
  req.message.set_request.general.node_id         = 77u;
  boomlink_ConfigMessage resp;
  handle(&svc, &req, &resp);
  REQUIRE(svc.apply_state == BOOMLINK_CONFIG_APPLY_STAGED, "setup: must be staged before commit");

  boomlink_config_service_commit_pending_apply(&svc, 1000u);
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_WAITING, "commit moves STAGED to WAITING");
  CHECK(svc.current.general.node_id == 77u, "commit is what actually applies the staged value");
  CHECK(svc.apply_started_at_ms == 1000u, "the confirmation window starts at commit time");
}

static void test_confirm_pending_apply_requires_waiting(void) {
  boomlink_config_service_t svc = make_svc(1000u);

  boomlink_config_service_confirm_pending_apply(&svc);
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_IDLE, "confirm with nothing waiting must be a no-op");

  boomlink_ConfigMessage req                      = {0};
  req.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version = 1u;
  req.message.set_request.has_general             = true;
  req.message.set_request.general.node_id         = 77u;
  boomlink_ConfigMessage resp;
  handle(&svc, &req, &resp);

  boomlink_config_service_confirm_pending_apply(&svc);
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_STAGED,
        "confirm before commit must be a no-op - STAGED is not WAITING");

  boomlink_config_service_commit_pending_apply(&svc, 1000u);
  boomlink_config_service_confirm_pending_apply(&svc);
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_IDLE, "confirm finalizes WAITING back to IDLE");
  CHECK(svc.current.general.node_id == 77u, "confirm must not revert the already-applied value");
}

static void test_poll_is_a_no_op_while_staged(void) {
  boomlink_config_service_t svc = make_svc(500u);

  boomlink_ConfigMessage req                      = {0};
  req.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version = 1u;
  req.message.set_request.has_general             = true;
  req.message.set_request.general.node_id         = 77u;
  boomlink_ConfigMessage resp;
  handle(&svc, &req, &resp);
  REQUIRE(svc.apply_state == BOOMLINK_CONFIG_APPLY_STAGED, "setup");

  /* The window has not started - apply_started_at_ms is still 0 from init,
     so a naive elapsed check against a huge now_ms would revert something
     that was never applied in the first place. */
  boomlink_config_service_poll(&svc, 1000000u);
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_STAGED,
        "poll must not touch a STAGED (not yet committed) change");
  CHECK(svc.current.general.node_id == 0u, "the staged value must still not be in current");
}

static void test_poll_reverts_exactly_at_the_window_boundary(void) {
  boomlink_config_service_t svc = make_svc(500u);

  boomlink_ConfigMessage req                      = {0};
  req.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version = 1u;
  req.message.set_request.has_general             = true;
  req.message.set_request.general.node_id         = 77u;
  boomlink_ConfigMessage resp;
  handle(&svc, &req, &resp);
  boomlink_config_service_commit_pending_apply(&svc, 1000u);
  REQUIRE(svc.current.general.node_id == 77u, "setup: the new value is live");

  boomlink_config_service_poll(&svc, 1499u); /* elapsed 499 < 500 */
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_WAITING,
        "must still be waiting just before the window closes");
  CHECK(svc.current.general.node_id == 77u, "must not revert early");

  boomlink_config_service_poll(&svc, 1500u); /* elapsed exactly 500 */
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_IDLE, "the window boundary itself must revert");
  CHECK(svc.current.general.node_id == 0u, "reverted to the pre-change value, got %u",
        (unsigned)svc.current.general.node_id);
}

static void test_a_second_set_while_one_is_pending_is_rejected(void) {
  boomlink_config_service_t svc = make_svc(500u);

  boomlink_ConfigMessage req1                      = {0};
  req1.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req1.message.set_request.expected_config_version = 1u;
  req1.message.set_request.has_general             = true;
  req1.message.set_request.general.node_id         = 77u;
  boomlink_ConfigMessage resp1;
  handle(&svc, &req1, &resp1);
  REQUIRE(resp1.message.set_response.result ==
              boomlink_ConfigSetResult_CONFIG_SET_RESULT_PENDING_CONFIRMATION,
          "setup: the first change must be pending");

  /* A second SET, even a purely non-hazardous one, arriving while the first
     is still STAGED. */
  boomlink_ConfigMessage req2                      = {0};
  req2.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req2.message.set_request.expected_config_version = 2u; /* the version req1 bumped to */
  req2.message.set_request.has_telemetry           = true;
  req2.message.set_request.telemetry.report_interval_s = 10u;
  boomlink_ConfigMessage resp2;
  bool ok2 = handle(&svc, &req2, &resp2);

  REQUIRE(ok2, "a SET always answers, even a rejected one");
  CHECK(resp2.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_APPLY_IN_PROGRESS,
        "a change must not be accepted while another is still pending");
  CHECK(svc.current.telemetry.report_interval_s == 0u,
        "the rejected SET must not have applied, even its non-hazardous field");
  CHECK(svc.staged.node_id == 77u, "the original pending change must be untouched");

  /* And still rejected once WAITING, not just while STAGED. */
  boomlink_config_service_commit_pending_apply(&svc, 1000u);
  boomlink_ConfigMessage resp3;
  bool ok3 = handle(&svc, &req2, &resp3); /* config_version did not move: req2 was rejected */
  CHECK(ok3, "a SET always answers");
  CHECK(resp3.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_APPLY_IN_PROGRESS,
        "still rejected while WAITING for confirmation, not just while STAGED");
}

static void test_get_config_is_null_tolerant(void) {
  boomlink_node_config_t out;
  boomlink_config_service_get_config(NULL, &out);
  CHECK(out.config_version == 0u, "a NULL service must yield a zeroed config, not a crash");

  boomlink_config_service_t svc = make_svc(1000u);
  svc.current.general.node_id   = 42u;
  boomlink_config_service_get_config(&svc, &out);
  CHECK(out.general.node_id == 42u, "a real service must be read out faithfully");
}

static void test_handle_rejects_malformed_or_missing_arguments(void) {
  boomlink_config_service_t   svc = make_svc(1000u);
  boomlink_dispatch_rx_info_t rx  = {0};
  boomlink_ConfigMessage      resp;

  boomlink_ConfigMessage not_a_request = {0};
  not_a_request.which_message = boomlink_ConfigMessage_get_response_tag; /* a decoded response */
  CHECK(!boomlink_config_service_handle(&svc, &rx, &not_a_request, &resp),
        "a decoded response is not something this service can answer");

  boomlink_ConfigMessage valid = get_request(true, false, false, false, false, false);
  CHECK(!boomlink_config_service_handle(NULL, &rx, &valid, &resp), "a NULL svc must be rejected");
  CHECK(!boomlink_config_service_handle(&svc, &rx, NULL, &resp), "a NULL request must be rejected");
  CHECK(!boomlink_config_service_handle(&svc, &rx, &valid, NULL),
        "a NULL out_response must be rejected");
}

int main(void) {
  test_get_returns_only_requested_groups();
  test_set_rejects_stale_expected_version();
  test_non_hazardous_set_applies_immediately_as_a_whole_group();
  test_set_leaves_omitted_groups_untouched();
  test_hazardous_node_id_change_is_staged_but_sibling_fields_apply_now();
  test_hazardous_magic_change_is_staged();
  test_hazardous_radio_change_touches_nothing_until_committed();
  test_requesting_the_current_hazardous_value_is_not_a_hazard_change();
  test_commit_pending_apply_requires_staged();
  test_confirm_pending_apply_requires_waiting();
  test_poll_is_a_no_op_while_staged();
  test_poll_reverts_exactly_at_the_window_boundary();
  test_a_second_set_while_one_is_pending_is_rejected();
  test_get_config_is_null_tolerant();
  test_handle_rejects_malformed_or_missing_arguments();
  BOOMLINK_TEST_REPORT("config_service_test", 74);
}
