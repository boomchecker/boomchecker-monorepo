/**
 ******************************************************************************
 * @file    boomlink_command_service.h
 * @brief   Section 8.3's command set: turns a CommandRequest into a typed
 *          CommandResponse by calling an INJECTED action for whichever
 *          command it names - the same seam pattern as boomlink_port_t,
 *          for the same reason: this file stays host-testable against a
 *          fake ops struct, and Phase C wires the real one (NVIC reset,
 *          an LED, the detection subsystem once one exists).
 ******************************************************************************
 */
#ifndef BOOMLINK_COMMAND_SERVICE_H
#define BOOMLINK_COMMAND_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include "boomlink_dispatch.h"
#include "command.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One callback per section 8.3 command. Any may be NULL - the matching
 * command then answers COMMAND_RESULT_UNSUPPORTED rather than crashing,
 * the same reasoning boomlink_link_config_t's optional on_rx/on_tx_done
 * follow.
 *
 * self_test/request_diagnostics may fill `out_diagnostic` (capacity
 * `out_diagnostic_cap`, already zeroed by the caller) with a short
 * human-readable note - section 13's "an optional bounded diagnostic
 * string... but the enum is authoritative". Bounded by
 * nanopb/command.options, not by this struct. `out_diagnostic_cap` already
 * has room for a terminating NUL reserved out of it, and the byte at
 * `out_diagnostic[out_diagnostic_cap - 1]` is forced to `'\0'` regardless of
 * what this callback writes - Nanopb's string encoder requires
 * NUL-termination within the field's declared size, and a callback that
 * fills every byte it was handed must not be able to make the response
 * fail to encode.
 *
 * **reboot() must NOT reset synchronously.** The CommandResponse this call
 * is building has not been sent yet - boomlink_command_service_handle()
 * returns it to its caller, which still has to encode and transmit it.  A
 * synchronous NVIC_SystemReset() inside reboot() would reset the MCU before
 * that response ever reaches the radio, so the requester waits for an ACK
 * that can never arrive and has no way to tell "rebooted" from "command
 * lost". reboot() must instead ARM a deferred reset - e.g. set a flag the
 * superloop checks on a LATER iteration, after this response has actually
 * gone out - which is Phase C's concern to implement against real hardware,
 * not this service's to enforce (it has no notion of "has been sent" to
 * enforce it against).
 *
 * **reboot() is never called for a broadcast request.** Section 9.9:
 * "commands that are dangerous when broadcast should be rejected by the
 * application service unless explicitly designed for broadcast" -
 * boomlink_command_service_handle() enforces this itself for Reboot
 * (answering COMMAND_RESULT_FAILED without invoking this callback at all),
 * since it is the only point in this chain that sees both `rx` and the
 * command type together.
 */
typedef struct {
  bool (*reboot)(void *ctx);
  bool (*identify)(void *ctx);
  bool (*self_test)(void *ctx, char *out_diagnostic, size_t out_diagnostic_cap);
  bool (*start_detection)(void *ctx);
  bool (*stop_detection)(void *ctx);
  bool (*clear_statistics)(void *ctx);
  bool (*request_diagnostics)(void *ctx, char *out_diagnostic, size_t out_diagnostic_cap);
  void *ctx;
} boomlink_command_ops_t;

/**
 * A boomlink_dispatch_command_fn: register with
 * `handlers.on_command = boomlink_command_service_handle` and
 * `handlers.on_command_user = &ops` (a `const boomlink_command_ops_t *`).
 *
 * Always returns true (a response is always built) when `request` actually
 * carries a CommandRequest - section 8.3's "commands... return a correlated
 * response with a typed result/error code" makes a response mandatory, not
 * conditional on the command being recognized: COMMAND_RESULT_UNSUPPORTED
 * IS that response for an unrecognized or unwired command, not the absence
 * of one. Returns false only if `request` holds no CommandRequest at all
 * (e.g. it decoded as a CommandResponse instead - a malformed exchange this
 * service cannot answer either way) or `user` is NULL.
 *
 * A Reboot request addressed to BOOMLINK_ADDR_BROADCAST (via `rx->
 * destination_id`) is answered COMMAND_RESULT_FAILED without ever calling
 * `ops->reboot` - see that field's own doc for why.
 */
bool boomlink_command_service_handle(void *user, const boomlink_dispatch_rx_info_t *rx,
                                     const boomlink_CommandMessage *request,
                                     boomlink_CommandMessage *out_response);

#ifdef __cplusplus
}
#endif

#endif /* BOOMLINK_COMMAND_SERVICE_H */
