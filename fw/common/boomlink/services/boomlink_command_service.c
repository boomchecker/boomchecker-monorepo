/**
 ******************************************************************************
 * @file    boomlink_command_service.c
 ******************************************************************************
 */
#include "boomlink_command_service.h"

#include <string.h>

/* For BOOMLINK_ADDR_BROADCAST (boomlink.md section 7.2) - reused rather
   than duplicated as a bare hex literal, the same reasoning boomlink_
   config_service.c gives for pulling in this same header. Header-only
   use, no Nanopb dependency of its own. */
#include "boomlink_linkframe.h"

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
  /* Reserve the last byte for a NUL the callback might not think to leave
     room for, then force it regardless of what the callback did: Nanopb's
     string encoder requires this field be NUL-terminated within its
     declared size, and a callback that (plausibly, if wrongly) fills the
     buffer edge-to-edge using the full advertised capacity would otherwise
     leave an unterminated string that fails the WHOLE response's encode
     later - not a diagnostic-field problem, a "the command silently never
     reaches the caller" problem. */
  *out_result = action(ctx, out_diagnostic, out_diagnostic_cap - 1)
                    ? boomlink_CommandResult_COMMAND_RESULT_OK
                    : boomlink_CommandResult_COMMAND_RESULT_FAILED;
  out_diagnostic[out_diagnostic_cap - 1] = '\0';
}

/* Section 9.9: "commands that are dangerous when broadcast should be
   rejected by the application service unless explicitly designed for
   broadcast." A table rather than one hardcoded `if` per command: Reboot
   was the first case fixed here, then review found ClearStatistics had the
   identical unguarded shape (one broadcast frame wiping every reachable
   node's diagnostic counters at once) - two independent instances of the
   same gap is exactly the pattern that keeps recurring one command at a
   time in this codebase's own review history, so this closes it structurally
   instead of waiting for a human to remember a third `if`.
   StartDetection/StopDetection are included pre-emptively: both are `NULL`
   in every ops struct wired today (no detection algorithm exists - see
   boomlink_command_ops_t's own doc), so this changes nothing observable
   yet, but the day a real detector IS wired behind them, a broadcast
   StopDetection silencing the whole fleet's detection from one frame would
   be exactly as severe as the Reboot case - this way that guard is already
   in place before the landmine can be stepped on, not after.
   Identify/SelfTest/RequestDiagnostics are deliberately NOT included: none
   of them change persistent state or safety-relevant behavior - a broadcast
   Identify is a nuisance (every node blinks, nothing breaks), and a
   broadcast SelfTest/RequestDiagnostics reply storm is the same "no
   per-peer rate limiting" risk boomlink.md's Phase C notes already name as
   a known, separately-tracked gap, not a new one this table should absorb. */
static bool command_is_dangerous_over_broadcast(boomlink_CommandType type) {
  switch (type) {
    case boomlink_CommandType_COMMAND_TYPE_REBOOT:
    case boomlink_CommandType_COMMAND_TYPE_CLEAR_STATISTICS:
    case boomlink_CommandType_COMMAND_TYPE_START_DETECTION:
    case boomlink_CommandType_COMMAND_TYPE_STOP_DETECTION:
      return true;
    default:
      return false;
  }
}

bool boomlink_command_service_handle(void *user, const boomlink_dispatch_rx_info_t *rx,
                                     const boomlink_CommandMessage *request,
                                     boomlink_CommandMessage *out_response) {
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

  /* This is the first point in the chain that has both `rx->destination_id`
     and the command type together: boomlink_dispatch_process() passes `rx`
     through unfiltered, and boomlink_command_ops_t's per-command callbacks
     (see that struct's own doc) do not receive it at all, so nothing
     "closer" to the actual action could enforce this instead.
     COMMAND_RESULT_FAILED, not UNSUPPORTED: this node CAN perform the
     command (a unicast request still works, when wired), it is this
     specific broadcast request that is refused, and UNSUPPORTED would
     misreport a capability the node genuinely has. The real ops callback
     is deliberately not called at all - not even to have it decline. */
  if (rx != NULL && rx->destination_id == BOOMLINK_ADDR_BROADCAST &&
      command_is_dangerous_over_broadcast(type)) {
    resp->result = boomlink_CommandResult_COMMAND_RESULT_FAILED;
    return true;
  }

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
