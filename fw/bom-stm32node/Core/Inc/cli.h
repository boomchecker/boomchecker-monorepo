/**
 ******************************************************************************
 * @file    cli.h
 * @brief   Interactive console on top of embedded-cli (funbiscuit).
 *
 * Transport-agnostic: output goes through a cli_tx_fn callback, input is fed
 * via cli_feed(). The USB CDC glue (CDC read/write) lives outside this module,
 * so cli.c has no dependency on the USB stack.
 ******************************************************************************
 */
#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Transport send: send `len` bytes.
 * @return 0 on success; non-zero if busy (cli_process retries the same block
 *         on the next iteration). For USB CDC this wraps the CDC write.
 */
typedef int (*cli_tx_fn)(const uint8_t *buf, uint16_t len);

/**
 * @brief Create the CLI (static buffer), register commands, print the prompt.
 * @param tx transport send callback (must not be NULL for output)
 */
void cli_init(cli_tx_fn tx);

/**
 * @brief Process commands and flush the TX buffer. Call in the main loop.
 */
void cli_process(void);

/**
 * @brief Feed received bytes to the CLI. Call from RX (e.g. CDC read).
 */
void cli_feed(const uint8_t *buf, uint32_t len);

#endif /* CLI_H */
