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

#include <math.h>

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

static void test_a_conflicting_hazardous_set_while_one_is_pending_is_rejected(void) {
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
  REQUIRE(resp1.message.set_response.config_version == 2u, "setup: version bumped to 2");

  /* A second SET that is itself non-hazardous, arriving while the first is
     still STAGED, must NOT be blocked - only an overlapping HAZARDOUS
     change conflicts (config.proto's own APPLY_IN_PROGRESS doc only
     discusses overlapping hazardous windows), and this PR's own hazard
     design keeps a non-hazardous write from ever disturbing a pending
     stage/revert. */
  boomlink_ConfigMessage req2                          = {0};
  req2.which_message                                   = boomlink_ConfigMessage_set_request_tag;
  req2.message.set_request.expected_config_version     = 2u;
  req2.message.set_request.has_telemetry               = true;
  req2.message.set_request.telemetry.report_interval_s = 10u;
  boomlink_ConfigMessage resp2;
  bool ok2 = handle(&svc, &req2, &resp2);

  REQUIRE(ok2, "a SET always answers");
  CHECK(resp2.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_OK,
        "a non-hazardous SET must apply even while an unrelated hazardous change is pending");
  CHECK(svc.current.telemetry.report_interval_s == 10u,
        "the non-hazardous field must actually have applied");
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_STAGED,
        "the pending hazardous change must be undisturbed - still STAGED");
  CHECK(svc.staged.node_id == 77u, "the original pending change must be untouched");

  /* A THIRD set that IS hazardous (a different node_id) must still be
     rejected while the first remains pending. */
  boomlink_ConfigMessage req3                      = {0};
  req3.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req3.message.set_request.expected_config_version = 3u; /* req2 bumped to 3 */
  req3.message.set_request.has_general             = true;
  req3.message.set_request.general.node_id         = 88u;
  boomlink_ConfigMessage resp3;
  bool ok3 = handle(&svc, &req3, &resp3);

  REQUIRE(ok3, "a SET always answers, even a rejected one");
  CHECK(resp3.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_APPLY_IN_PROGRESS,
        "a conflicting hazardous change must still be rejected while one is pending");
  CHECK(svc.staged.node_id == 77u, "the original pending change must survive the rejected attempt");
  CHECK(svc.current.config_version == 3u, "a rejected SET must not bump the version");

  /* And the same two outcomes hold once WAITING, not just while STAGED. */
  boomlink_config_service_commit_pending_apply(&svc, 1000u);
  REQUIRE(svc.apply_state == BOOMLINK_CONFIG_APPLY_WAITING, "setup: now waiting for confirmation");

  boomlink_ConfigMessage req4                      = {0};
  req4.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req4.message.set_request.expected_config_version = 3u;
  req4.message.set_request.has_gnss                = true;
  req4.message.set_request.gnss.gnss_enabled       = true;
  boomlink_ConfigMessage resp4;
  handle(&svc, &req4, &resp4);
  CHECK(resp4.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_OK,
        "a non-hazardous SET must still apply while WAITING for a hazardous confirmation");
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_WAITING, "must still be waiting afterwards");

  boomlink_ConfigMessage req5                      = {0};
  req5.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req5.message.set_request.expected_config_version = 4u; /* req4 bumped to 4 */
  req5.message.set_request.has_link                = true;
  req5.message.set_request.link.magic              = 0xCDu;
  boomlink_ConfigMessage resp5;
  handle(&svc, &req5, &resp5);
  CHECK(resp5.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_APPLY_IN_PROGRESS,
        "a hazardous SET must still be rejected while WAITING, not just while STAGED");
}

static void test_set_rejects_an_attempt_to_change_node_id_to_an_invalid_value(void) {
  boomlink_config_service_t svc = make_svc(1000u);

  boomlink_ConfigMessage req                      = {0};
  req.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version = 1u;
  req.message.set_request.has_general             = true;
  req.message.set_request.general.node_id         = 0xFFFFFFFFu; /* the broadcast address */

  boomlink_ConfigMessage resp;
  bool ok = handle(&svc, &req, &resp);

  REQUIRE(ok, "a SET always answers, even a rejected one");
  CHECK(resp.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_INVALID,
        "a node's own identity must never become the broadcast address");
  CHECK(svc.current.config_version == 1u, "an invalid SET must not bump the version");
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_IDLE, "an invalid SET must not stage anything");

  /* 0x00000000 (section 7.2's "unconfigured") is equally invalid as an
     explicit CHANGE target, not just the broadcast address - starting from
     a real, already-assigned node_id this time (55), since validation
     checks the delta, not presence: SETting an already-0 node_id back to 0
     is a no-op resend, covered separately by
     test_resending_an_unchanged_but_still_unconfigured_node_id_is_not_rejected. */
  svc.current.general.node_id             = 55u;
  req.message.set_request.general.node_id = 0u;
  ok                                       = handle(&svc, &req, &resp);
  CHECK(ok, "a SET always answers");
  CHECK(resp.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_INVALID,
        "0x00000000 is unconfigured, not a value a node may CHANGE its identity to");
  CHECK(svc.current.general.node_id == 55u,
        "the rejected change must not have touched the real node_id");
}

static void test_set_rejects_an_attempt_to_change_magic_past_one_byte(void) {
  boomlink_config_service_t svc = make_svc(1000u);

  boomlink_ConfigMessage req                      = {0};
  req.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version = 1u;
  req.message.set_request.has_link                = true;
  req.message.set_request.link.magic              = 256u; /* one past what fits the wire byte */

  boomlink_ConfigMessage resp;
  handle(&svc, &req, &resp);
  CHECK(resp.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_INVALID,
        "magic is one wire byte (section 7.3) - 256 does not fit it");

  /* The boundary itself (255) must NOT be rejected. */
  req.message.set_request.link.magic = 255u;
  handle(&svc, &req, &resp);
  CHECK(resp.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_PENDING_CONFIRMATION,
        "255 fits exactly in one byte and must be accepted as a real hazardous change");
}

static void test_resending_an_unchanged_but_still_unconfigured_node_id_is_not_rejected(void) {
  /* A fresh node's node_id defaults to 0x00000000 (section 7.2's
     "unconfigured"). A caller editing an unrelated GeneralConfig field
     before the node has ever been assigned a real identity must be able to
     resend that same 0 untouched, per the whole-group-replacement contract
     - validation only concerns a CHANGE to an invalid value, not a resend
     of one that was already there. */
  boomlink_config_service_t svc = make_svc(1000u);
  REQUIRE(svc.current.general.node_id == 0u, "setup: a fresh node has no identity yet");

  boomlink_ConfigMessage req                              = {0};
  req.which_message                                       = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version         = 1u;
  req.message.set_request.has_general                     = true;
  req.message.set_request.general.node_id                 = 0u; /* unchanged */
  req.message.set_request.general.default_destination_id  = 5u; /* the actual edit */

  boomlink_ConfigMessage resp;
  bool ok = handle(&svc, &req, &resp);

  REQUIRE(ok, "a SET always answers");
  CHECK(resp.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_OK,
        "resending the same (still-invalid) node_id must not itself be rejected");
  CHECK(svc.current.general.default_destination_id == 5u, "the actual edit must have applied");
}

static void test_radio_negative_zero_is_not_a_hazardous_change(void) {
  /* memcmp()-based float comparison would see +0.0f and -0.0f as different
     bit patterns and misreport this as a hazard change, even though IEEE-754
     equality (and every real consumer of frequency_mhz) treats them as the
     same value. */
  boomlink_config_service_t svc   = make_svc(1000u);
  svc.current.radio.frequency_mhz = 0.0f;

  boomlink_ConfigMessage req                      = {0};
  req.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version = 1u;
  req.message.set_request.has_radio               = true;
  req.message.set_request.radio.frequency_mhz     = -0.0f;

  boomlink_ConfigMessage resp;
  handle(&svc, &req, &resp);

  CHECK(resp.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_OK,
        "+0.0f and -0.0f must compare equal for hazard purposes, not stage a phantom change");
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_IDLE, "nothing should be pending");
}

static void test_radio_nan_does_not_permanently_break_hazard_detection(void) {
  /* RadioConfig's numeric ranges are deliberately not validated yet
     (config.proto's own CONFIG_SET_RESULT_INVALID comment), so a NaN CAN
     legitimately end up in current.radio. Plain `==` is not reflexive for
     NaN (NaN == NaN is false), and hazard comparison relies on
     SELF-comparison for every field a SET leaves untouched - so once a NaN
     landed in current, every later SET, however unrelated, would misreport
     hazard_changed=true forever, with no timeout recovery (poll() does not
     revert a merely-STAGED, uncommitted change - see
     test_poll_is_a_no_op_while_staged). */
  boomlink_config_service_t svc = make_svc(1000u);

  boomlink_ConfigMessage req                      = {0};
  req.which_message                               = boomlink_ConfigMessage_set_request_tag;
  req.message.set_request.expected_config_version = 1u;
  req.message.set_request.has_radio               = true;
  req.message.set_request.radio.frequency_mhz     = NAN;
  boomlink_ConfigMessage resp;
  handle(&svc, &req, &resp);
  REQUIRE(resp.message.set_response.result ==
              boomlink_ConfigSetResult_CONFIG_SET_RESULT_PENDING_CONFIRMATION,
          "setup: staging a NaN frequency is itself a real hazard change from the default 0.0f");
  boomlink_config_service_commit_pending_apply(&svc, 100u);
  boomlink_config_service_confirm_pending_apply(&svc);
  REQUIRE(svc.apply_state == BOOMLINK_CONFIG_APPLY_IDLE, "setup: the NaN is now permanently applied");
  REQUIRE(isnan(svc.current.radio.frequency_mhz), "setup: confirm it actually landed as NaN");

  /* A completely unrelated, non-hazardous SET afterwards must not be stuck
     misreporting a phantom hazard forever. */
  boomlink_ConfigMessage req2                          = {0};
  req2.which_message                                   = boomlink_ConfigMessage_set_request_tag;
  req2.message.set_request.expected_config_version     = 2u;
  req2.message.set_request.has_telemetry               = true;
  req2.message.set_request.telemetry.report_interval_s = 30u;
  boomlink_ConfigMessage resp2;
  handle(&svc, &req2, &resp2);
  CHECK(resp2.message.set_response.result == boomlink_ConfigSetResult_CONFIG_SET_RESULT_OK,
        "a NaN sitting in current.radio must not misreport every later SET as hazardous");
  CHECK(svc.apply_state == BOOMLINK_CONFIG_APPLY_IDLE, "nothing should be pending");
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
  test_a_conflicting_hazardous_set_while_one_is_pending_is_rejected();
  test_set_rejects_an_attempt_to_change_node_id_to_an_invalid_value();
  test_set_rejects_an_attempt_to_change_magic_past_one_byte();
  test_resending_an_unchanged_but_still_unconfigured_node_id_is_not_rejected();
  test_radio_negative_zero_is_not_a_hazardous_change();
  test_radio_nan_does_not_permanently_break_hazard_detection();
  test_get_config_is_null_tolerant();
  test_handle_rejects_malformed_or_missing_arguments();
  BOOMLINK_TEST_REPORT("config_service_test", 102);
}
