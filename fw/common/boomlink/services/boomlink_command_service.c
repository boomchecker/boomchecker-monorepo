/**
 ******************************************************************************
 * @file    boomlink_command_service.c
 ******************************************************************************
 */
#include "boomlink_command_service.h"

#include <string.h>

/* Calls `action` (may be NULL) and sets *out_result to UNSUPPORTED,
   FAILED or OK accordingly. Shared by every command that takes no
   diagnostic string, so the five identical branches below stay branches,
   not five near-copies of this same three-line dispatch. */
static void run_simple(bool (*action)(void *), void *ctx, boomlink_CommandResult *out_result) {
  if (action == NULL) {
    *out_result = boomlink_CommandResult_COMMAND_RESULT_UNSUPPORTED;
    return;
  }
  *out_result = action(ctx) ? boomlink_CommandResult_COMMAND_RESULT_OK
                            : boomlink_CommandResult_COMMAND_RESULT_FAILED;
}

static void run_diagnostic(bool (*action)(void *, char *, size_t), void *ctx,
                           boomlink_CommandResult *out_result, char *out_diagnostic,
                           size_t out_diagnostic_cap) {
  if (action == NULL) {
    *out_result = boomlink_CommandResult_COMMAND_RESULT_UNSUPPORTED;
    return;
  }
  *out_result = action(ctx, out_diagnostic, out_diagnostic_cap)
                    ? boomlink_CommandResult_COMMAND_RESULT_OK
                    : boomlink_CommandResult_COMMAND_RESULT_FAILED;
}

bool boomlink_command_service_handle(void *user, const boomlink_dispatch_rx_info_t *rx,
                                     const boomlink_CommandMessage *request,
                                     boomlink_CommandMessage *out_response) {
  (void)rx;
  const boomlink_command_ops_t *ops = (const boomlink_command_ops_t *)user;
  if (ops == NULL || out_response == NULL || request == NULL ||
      request->which_message != boomlink_CommandMessage_request_tag) {
    return false;
  }

  boomlink_CommandType type = request->message.request.type;

  out_response->which_message           = boomlink_CommandMessage_response_tag;
  boomlink_CommandResponse *resp        = &out_response->message.response;
  resp->type                            = type;
  resp->result                          = boomlink_CommandResult_COMMAND_RESULT_UNSUPPORTED;
  memset(resp->diagnostic, 0, sizeof(resp->diagnostic));

  switch (type) {
    case boomlink_CommandType_COMMAND_TYPE_REBOOT:
      run_simple(ops->reboot, ops->ctx, &resp->result);
      break;
    case boomlink_CommandType_COMMAND_TYPE_IDENTIFY:
      run_simple(ops->identify, ops->ctx, &resp->result);
      break;
    case boomlink_CommandType_COMMAND_TYPE_SELF_TEST:
      run_diagnostic(ops->self_test, ops->ctx, &resp->result, resp->diagnostic,
                     sizeof(resp->diagnostic));
      break;
    case boomlink_CommandType_COMMAND_TYPE_START_DETECTION:
      run_simple(ops->start_detection, ops->ctx, &resp->result);
      break;
    case boomlink_CommandType_COMMAND_TYPE_STOP_DETECTION:
      run_simple(ops->stop_detection, ops->ctx, &resp->result);
      break;
    case boomlink_CommandType_COMMAND_TYPE_CLEAR_STATISTICS:
      run_simple(ops->clear_statistics, ops->ctx, &resp->result);
      break;
    case boomlink_CommandType_COMMAND_TYPE_REQUEST_DIAGNOSTICS:
      run_diagnostic(ops->request_diagnostics, ops->ctx, &resp->result, resp->diagnostic,
                     sizeof(resp->diagnostic));
      break;
    case boomlink_CommandType_COMMAND_TYPE_UNSPECIFIED:
    default:
      /* Already UNSUPPORTED from the initializer above - an unrecognized
         type (proto3's zero value, or a future type this build predates)
         gets the same answer as a recognized-but-unwired one, which is the
         honest thing to say either way: this build cannot do it. */
      break;
  }

  return true;
}
