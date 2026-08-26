/**
 ******************************************************************************
 * @file    dispatch_test.c
 * @brief   Tests for boomlink_dispatch_process() (dispatch/boomlink_dispatch.h).
 *
 *          The guarantee under test: every decoded Envelope reaches exactly
 *          the one handler its which_payload names, a request/response
 *          handler's returned response gets a header this module built
 *          itself - never the handler's job, per the function's own doc -
 *          and every outcome (routed, no handler wired, no payload at all)
 *          is counted, never silently dropped.
 ******************************************************************************
 */
#include "boomlink_dispatch.h"

#include <string.h>

#include "boomlink_codec.h"
#include "boomlink_envelope_builder.h"
#include "c_test.h"

BOOMLINK_TEST_STATE;

typedef struct {
  int                          calls;
  boomlink_dispatch_rx_info_t  rx;
  boomlink_DetectionMessage    msg;
} detection_capture_t;

static void on_detection(void *user, const boomlink_dispatch_rx_info_t *rx,
                         const boomlink_DetectionMessage *msg) {
  detection_capture_t *cap = (detection_capture_t *)user;
  cap->calls++;
  cap->rx  = *rx;
  cap->msg = *msg;
}

typedef struct {
  int                          calls;
  boomlink_dispatch_rx_info_t  rx;
  boomlink_TelemetryMessage    msg;
} telemetry_capture_t;

static void on_telemetry(void *user, const boomlink_dispatch_rx_info_t *rx,
                         const boomlink_TelemetryMessage *msg) {
  telemetry_capture_t *cap = (telemetry_capture_t *)user;
  cap->calls++;
  cap->rx  = *rx;
  cap->msg = *msg;
}

/* One capture/response-to-return pair per request/response group, so a
   scenario controls what the handler answers and can inspect what it saw. */
typedef struct {
  int                     calls;
  bool                    result;
  boomlink_CommandMessage response;
} command_capture_t;

static bool on_command(void *user, const boomlink_dispatch_rx_info_t *rx,
                       const boomlink_CommandMessage *request,
                       boomlink_CommandMessage *out_response) {
  (void)rx;
  (void)request;
  command_capture_t *cap = (command_capture_t *)user;
  cap->calls++;
  *out_response = cap->response;
  return cap->result;
}

typedef struct {
  int                    calls;
  bool                   result;
  boomlink_ConfigMessage response;
} config_capture_t;

static bool on_config(void *user, const boomlink_dispatch_rx_info_t *rx,
                      const boomlink_ConfigMessage *request,
                      boomlink_ConfigMessage *out_response) {
  (void)rx;
  (void)request;
  config_capture_t *cap = (config_capture_t *)user;
  cap->calls++;
  *out_response = cap->response;
  return cap->result;
}

typedef struct {
  int                    calls;
  bool                   result;
  boomlink_SystemMessage response;
} system_capture_t;

static bool on_system(void *user, const boomlink_dispatch_rx_info_t *rx,
                      const boomlink_SystemMessage *request,
                      boomlink_SystemMessage *out_response) {
  (void)rx;
  (void)request;
  system_capture_t *cap = (system_capture_t *)user;
  cap->calls++;
  *out_response = cap->response;
  return cap->result;
}

static boomlink_dispatch_rx_info_t make_rx(void) {
  boomlink_dispatch_rx_info_t rx = {0};
  rx.source_id = 0x42u;
  rx.rssi_dbm  = -80.0f;
  rx.snr_db    = 7.5f;
  return rx;
}

static boomlink_Envelope make_request(uint32_t request_id) {
  boomlink_Envelope env;
  boomlink_envelope_init(&env);
  env.header.request_id = request_id;
  return env;
}

static void test_detection_is_one_way_and_counted(void) {
  detection_capture_t cap = {0};
  boomlink_dispatch_handlers_t h = {0};
  h.on_detection      = on_detection;
  h.on_detection_user = &cap;
  boomlink_dispatch_t d;
  boomlink_dispatch_init(&d, &h);

  boomlink_Envelope env                        = make_request(1u);
  env.which_payload                            = boomlink_Envelope_detection_tag;
  env.payload.detection.which_message          = boomlink_DetectionMessage_event_tag;
  env.payload.detection.message.event.event_id = 7u;

  boomlink_dispatch_rx_info_t rx = make_rx();
  boomlink_dispatch_result_t result = boomlink_dispatch_process(&d, &rx, &env);

  CHECK(cap.calls == 1, "detection handler called once, got %d", cap.calls);
  CHECK(cap.msg.message.event.event_id == 7u, "the handler must see the actual event");
  CHECK(cap.rx.source_id == rx.source_id, "rx info must be forwarded unchanged");
  CHECK(!result.has_response, "detection is one-way, never a response");

  boomlink_dispatch_stats_t stats;
  boomlink_dispatch_get_stats(&d, &stats);
  CHECK(stats.detection_received == 1u, "detection_received must be counted, got %u",
        (unsigned)stats.detection_received);
  CHECK(stats.no_payload == 0u && stats.unhandled == 0u, "no spurious counts");
}

static void test_telemetry_is_one_way_and_counted(void) {
  telemetry_capture_t cap = {0};
  boomlink_dispatch_handlers_t h = {0};
  h.on_telemetry      = on_telemetry;
  h.on_telemetry_user = &cap;
  boomlink_dispatch_t d;
  boomlink_dispatch_init(&d, &h);

  boomlink_Envelope env                          = make_request(2u);
  env.which_payload                              = boomlink_Envelope_telemetry_tag;
  env.payload.telemetry.which_message            = boomlink_TelemetryMessage_report_tag;
  env.payload.telemetry.message.report.uptime_s  = 123u;

  boomlink_dispatch_rx_info_t rx = make_rx();
  boomlink_dispatch_result_t result = boomlink_dispatch_process(&d, &rx, &env);

  CHECK(cap.calls == 1, "telemetry handler called once");
  CHECK(cap.msg.message.report.uptime_s == 123u, "the handler must see the actual report");
  CHECK(!result.has_response, "telemetry is one-way, never a response");

  boomlink_dispatch_stats_t stats;
  boomlink_dispatch_get_stats(&d, &stats);
  CHECK(stats.telemetry_received == 1u, "telemetry_received must be counted");
}

static void test_command_response_header_is_built_by_the_dispatcher(void) {
  command_capture_t cap = {0};
  cap.result                                        = true;
  cap.response.which_message                        = boomlink_CommandMessage_response_tag;
  cap.response.message.response.result              = boomlink_CommandResult_COMMAND_RESULT_OK;
  boomlink_dispatch_handlers_t h = {0};
  h.on_command      = on_command;
  h.on_command_user = &cap;
  boomlink_dispatch_t d;
  boomlink_dispatch_init(&d, &h);

  boomlink_Envelope env                          = make_request(4242u);
  env.which_payload                              = boomlink_Envelope_command_tag;
  env.payload.command.which_message              = boomlink_CommandMessage_request_tag;
  env.payload.command.message.request.type       = boomlink_CommandType_COMMAND_TYPE_IDENTIFY;

  boomlink_dispatch_rx_info_t rx = make_rx();
  boomlink_dispatch_result_t result = boomlink_dispatch_process(&d, &rx, &env);

  CHECK(cap.calls == 1, "command handler called once");
  REQUIRE(result.has_response, "a true return must produce a response");
  CHECK(result.response.has_header, "the dispatcher, not the handler, must set has_header");
  CHECK(result.response.header.protocol_version == BOOMLINK_PROTOCOL_VERSION,
        "protocol_version must be the dispatcher's concern, not the handler's");
  CHECK(result.response.header.request_id == 4242u,
        "request_id must correlate with the request, got %u",
        (unsigned)result.response.header.request_id);
  CHECK(result.response.which_payload == boomlink_Envelope_command_tag,
        "the response payload tag must be command");
  CHECK(result.response.payload.command.message.response.result ==
            boomlink_CommandResult_COMMAND_RESULT_OK,
        "the handler's own response content must survive unchanged");

  boomlink_dispatch_stats_t stats;
  boomlink_dispatch_get_stats(&d, &stats);
  CHECK(stats.command_received == 1u, "command_received must be counted regardless of the answer");
}

static void test_command_handler_returning_false_yields_no_response(void) {
  command_capture_t cap = {0};
  cap.result = false;
  boomlink_dispatch_handlers_t h = {0};
  h.on_command      = on_command;
  h.on_command_user = &cap;
  boomlink_dispatch_t d;
  boomlink_dispatch_init(&d, &h);

  boomlink_Envelope env                    = make_request(1u);
  env.which_payload                        = boomlink_Envelope_command_tag;
  env.payload.command.which_message        = boomlink_CommandMessage_request_tag;

  boomlink_dispatch_rx_info_t rx = make_rx();
  boomlink_dispatch_result_t result = boomlink_dispatch_process(&d, &rx, &env);

  CHECK(cap.calls == 1, "the handler must still be called");
  CHECK(!result.has_response, "false means no response, however the handler was called");
  CHECK(!result.response.has_header,
        "an unused response must be left fully zeroed, not a half-built Envelope");

  boomlink_dispatch_stats_t stats;
  boomlink_dispatch_get_stats(&d, &stats);
  CHECK(stats.command_received == 1u, "the attempt is still counted as received");
}

static void test_config_response_header_is_built_by_the_dispatcher(void) {
  config_capture_t cap = {0};
  cap.result                            = true;
  cap.response.which_message            = boomlink_ConfigMessage_set_response_tag;
  cap.response.message.set_response.result = boomlink_ConfigSetResult_CONFIG_SET_RESULT_OK;
  boomlink_dispatch_handlers_t h = {0};
  h.on_config      = on_config;
  h.on_config_user = &cap;
  boomlink_dispatch_t d;
  boomlink_dispatch_init(&d, &h);

  boomlink_Envelope env                    = make_request(99u);
  env.which_payload                        = boomlink_Envelope_config_tag;
  env.payload.config.which_message         = boomlink_ConfigMessage_set_request_tag;

  boomlink_dispatch_rx_info_t rx = make_rx();
  boomlink_dispatch_result_t result = boomlink_dispatch_process(&d, &rx, &env);

  CHECK(cap.calls == 1, "config handler called once");
  REQUIRE(result.has_response, "a true return must produce a response");
  CHECK(result.response.header.request_id == 99u, "request_id must correlate");
  CHECK(result.response.which_payload == boomlink_Envelope_config_tag,
        "the response payload tag must be config");
  CHECK(result.response.payload.config.message.set_response.result ==
            boomlink_ConfigSetResult_CONFIG_SET_RESULT_OK,
        "the handler's own response content must survive unchanged");
}

static void test_system_response_header_is_built_by_the_dispatcher(void) {
  system_capture_t cap = {0};
  cap.result                     = true;
  cap.response.which_message     = boomlink_SystemMessage_pong_tag;
  boomlink_dispatch_handlers_t h = {0};
  h.on_system      = on_system;
  h.on_system_user = &cap;
  boomlink_dispatch_t d;
  boomlink_dispatch_init(&d, &h);

  boomlink_Envelope env                   = make_request(5u);
  env.which_payload                       = boomlink_Envelope_system_tag;
  env.payload.system.which_message        = boomlink_SystemMessage_ping_tag;

  boomlink_dispatch_rx_info_t rx = make_rx();
  boomlink_dispatch_result_t result = boomlink_dispatch_process(&d, &rx, &env);

  CHECK(cap.calls == 1, "system handler called once");
  REQUIRE(result.has_response, "a true return must produce a response");
  CHECK(result.response.which_payload == boomlink_Envelope_system_tag,
        "the response payload tag must be system");
  CHECK(result.response.payload.system.which_message == boomlink_SystemMessage_pong_tag,
        "the handler's own response content must survive unchanged");

  boomlink_dispatch_stats_t stats;
  boomlink_dispatch_get_stats(&d, &stats);
  CHECK(stats.system_received == 1u, "system_received must be counted");
}

/* Every optional handler left NULL - the "a non-gateway node with no
   on_telemetry still counts a peer's report" case the header documents. */
static void test_every_group_with_no_handler_is_unhandled_not_dropped(void) {
  boomlink_dispatch_handlers_t h = {0}; /* nothing wired */
  boomlink_dispatch_t d;
  boomlink_dispatch_init(&d, &h);
  boomlink_dispatch_rx_info_t rx = make_rx();

  boomlink_Envelope detection = make_request(1u);
  detection.which_payload     = boomlink_Envelope_detection_tag;
  boomlink_dispatch_process(&d, &rx, &detection);

  boomlink_Envelope telemetry = make_request(1u);
  telemetry.which_payload     = boomlink_Envelope_telemetry_tag;
  boomlink_dispatch_process(&d, &rx, &telemetry);

  boomlink_Envelope command = make_request(1u);
  command.which_payload     = boomlink_Envelope_command_tag;
  boomlink_dispatch_result_t command_result = boomlink_dispatch_process(&d, &rx, &command);

  boomlink_Envelope config = make_request(1u);
  config.which_payload     = boomlink_Envelope_config_tag;
  boomlink_dispatch_result_t config_result = boomlink_dispatch_process(&d, &rx, &config);

  boomlink_Envelope system = make_request(1u);
  system.which_payload     = boomlink_Envelope_system_tag;
  boomlink_dispatch_result_t system_result = boomlink_dispatch_process(&d, &rx, &system);

  CHECK(!command_result.has_response, "an unhandled request/response group must not answer");
  CHECK(!config_result.has_response, "same for config");
  CHECK(!system_result.has_response, "same for system");

  boomlink_dispatch_stats_t stats;
  boomlink_dispatch_get_stats(&d, &stats);
  CHECK(stats.unhandled == 5u, "all five groups must be counted as unhandled, got %u",
        (unsigned)stats.unhandled);
  CHECK(stats.detection_received == 1u && stats.telemetry_received == 1u &&
            stats.command_received == 1u && stats.config_received == 1u &&
            stats.system_received == 1u,
        "each group's own received counter must still increment even with no handler wired");
  CHECK(stats.no_payload == 0u, "an unhandled KNOWN group is not the same as no payload at all");
}

static void test_no_payload_is_a_distinct_count_from_unhandled(void) {
  boomlink_dispatch_handlers_t h = {0};
  boomlink_dispatch_t d;
  boomlink_dispatch_init(&d, &h);

  /* boomlink_envelope_init() leaves which_payload at its unset default (the
     proto3 oneof-not-set value) - a header-only Envelope is wire-valid per
     boomlink_codec.h's own doc, so this is the realistic way to reach it,
     not a hand-forged out-of-range tag. */
  boomlink_Envelope env = make_request(1u);
  boomlink_dispatch_rx_info_t rx = make_rx();
  boomlink_dispatch_result_t result = boomlink_dispatch_process(&d, &rx, &env);

  CHECK(!result.has_response, "no payload means no response");

  boomlink_dispatch_stats_t stats;
  boomlink_dispatch_get_stats(&d, &stats);
  CHECK(stats.no_payload == 1u, "no_payload must be counted, got %u", (unsigned)stats.no_payload);
  CHECK(stats.unhandled == 0u,
        "no_payload and unhandled must stay separate signals - one is 'nothing to route', the "
        "other is 'we knew what this was and could not act on it'");
}

static void test_get_stats_is_null_tolerant(void) {
  boomlink_dispatch_stats_t out;
  boomlink_dispatch_get_stats(NULL, &out);
  CHECK(out.detection_received == 0u && out.no_payload == 0u,
        "a NULL dispatch must yield zeroed stats, not a crash");

  /* And a NULL `out` must not crash either, on a real dispatch. */
  boomlink_dispatch_handlers_t h = {0};
  boomlink_dispatch_t d;
  boomlink_dispatch_init(&d, &h);
  boomlink_dispatch_get_stats(&d, NULL);
}

static void test_init_resets_stats_even_over_stale_memory(void) {
  boomlink_dispatch_handlers_t h = {0};
  boomlink_dispatch_t d;
  memset(&d, 0xAA, sizeof(d)); /* simulate stack garbage, not a freshly-zeroed struct */
  boomlink_dispatch_init(&d, &h);

  boomlink_dispatch_stats_t stats;
  boomlink_dispatch_get_stats(&d, &stats);
  CHECK(stats.detection_received == 0u && stats.telemetry_received == 0u &&
            stats.command_received == 0u && stats.config_received == 0u &&
            stats.system_received == 0u && stats.no_payload == 0u && stats.unhandled == 0u,
        "init must zero every counter, not just the ones this run happens to touch");
}

/* Every other test in this file drives the dispatcher on hand-built
   in-memory structs, never through Nanopb's actual wire encoding. This is
   the one place PR 4 Phase A's four new message groups get a real
   boomlink_encode_envelope()+boomlink_decode_envelope() round trip -
   catching a .proto/.options misconfiguration long before Phase C's real
   wiring would need to. */
static void test_new_message_groups_survive_a_real_nanopb_round_trip(void) {
  uint8_t buf[boomlink_Envelope_size];
  size_t  len;

  boomlink_DetectionEvent event = {0};
  event.event_id                = 99u;
  event.type                    = boomlink_DetectionType_DETECTION_TYPE_GUNSHOT;
  event.confidence_percent      = 87u;
  boomlink_Envelope detection_env;
  boomlink_build_detection_event(&detection_env, &event);
  REQUIRE(boomlink_encode_envelope(&detection_env, buf, sizeof(buf), &len),
          "a populated DetectionEvent must encode");
  boomlink_Envelope decoded_detection;
  REQUIRE(boomlink_decode_envelope(buf, len, &decoded_detection), "and decode back");
  CHECK(decoded_detection.which_payload == boomlink_Envelope_detection_tag,
        "the payload tag must survive the wire");
  CHECK(decoded_detection.payload.detection.message.event.event_id == 99u,
        "event_id must survive the wire");
  CHECK(decoded_detection.payload.detection.message.event.type ==
            boomlink_DetectionType_DETECTION_TYPE_GUNSHOT,
        "type must survive the wire");
  CHECK(decoded_detection.payload.detection.message.event.confidence_percent == 87u,
        "confidence_percent must survive the wire");

  boomlink_TelemetryReport report = {0};
  report.uptime_s                 = 12345u;
  report.tx_packets               = 7u;
  boomlink_Envelope telemetry_env;
  boomlink_build_telemetry_report(&telemetry_env, &report);
  REQUIRE(boomlink_encode_envelope(&telemetry_env, buf, sizeof(buf), &len),
          "a populated TelemetryReport must encode");
  boomlink_Envelope decoded_telemetry;
  REQUIRE(boomlink_decode_envelope(buf, len, &decoded_telemetry), "and decode back");
  CHECK(decoded_telemetry.payload.telemetry.message.report.uptime_s == 12345u,
        "uptime_s must survive the wire");
  CHECK(decoded_telemetry.payload.telemetry.message.report.tx_packets == 7u,
        "tx_packets must survive the wire");

  boomlink_Envelope command_env;
  boomlink_envelope_init(&command_env);
  command_env.header.request_id                   = 55u;
  command_env.which_payload                        = boomlink_Envelope_command_tag;
  command_env.payload.command.which_message        = boomlink_CommandMessage_request_tag;
  command_env.payload.command.message.request.type = boomlink_CommandType_COMMAND_TYPE_REBOOT;
  REQUIRE(boomlink_encode_envelope(&command_env, buf, sizeof(buf), &len),
          "a CommandRequest must encode");
  boomlink_Envelope decoded_command;
  REQUIRE(boomlink_decode_envelope(buf, len, &decoded_command), "and decode back");
  CHECK(decoded_command.header.request_id == 55u, "request_id must survive the wire");
  CHECK(decoded_command.payload.command.message.request.type ==
            boomlink_CommandType_COMMAND_TYPE_REBOOT,
        "the command type must survive the wire");

  boomlink_Envelope config_env;
  boomlink_envelope_init(&config_env);
  config_env.which_payload                                     = boomlink_Envelope_config_tag;
  config_env.payload.config.which_message                      = boomlink_ConfigMessage_get_request_tag;
  config_env.payload.config.message.get_request.include_radio  = true;
  REQUIRE(boomlink_encode_envelope(&config_env, buf, sizeof(buf), &len),
          "a ConfigGetRequest must encode");
  boomlink_Envelope decoded_config;
  REQUIRE(boomlink_decode_envelope(buf, len, &decoded_config), "and decode back");
  CHECK(decoded_config.payload.config.message.get_request.include_radio,
        "include_radio must survive the wire");
  CHECK(!decoded_config.payload.config.message.get_request.include_general,
        "an unset bool field must decode back false, not leak stale memory");
}

int main(void) {
  test_detection_is_one_way_and_counted();
  test_telemetry_is_one_way_and_counted();
  test_command_response_header_is_built_by_the_dispatcher();
  test_command_handler_returning_false_yields_no_response();
  test_config_response_header_is_built_by_the_dispatcher();
  test_system_response_header_is_built_by_the_dispatcher();
  test_every_group_with_no_handler_is_unhandled_not_dropped();
  test_no_payload_is_a_distinct_count_from_unhandled();
  test_get_stats_is_null_tolerant();
  test_init_resets_stats_even_over_stale_memory();
  test_new_message_groups_survive_a_real_nanopb_round_trip();
  BOOMLINK_TEST_REPORT("dispatch_test", 61);
}
