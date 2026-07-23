/**
 ******************************************************************************
 * @file    cli.c
 * @brief   Console on top of embedded-cli with output via a transport callback.
 ******************************************************************************
 */
#include "cli.h"
#include "embedded_cli.h"
#include "main.h"   /* Error_Handler */

/* Static CLI allocation (no malloc). Sized for the rx/cmd/history below plus bindings. */
#define CLI_STATIC_BYTES  2048u
#define CLI_TX_RING       512u

static EmbeddedCli *s_cli;
static CLI_UINT     s_cli_buffer[BYTES_TO_CLI_UINTS(CLI_STATIC_BYTES)];
static cli_tx_fn    s_tx;

/* TX ring: writeChar fills it byte by byte, cli_process flushes in chunks. */
static uint8_t  s_tx_ring[CLI_TX_RING];
static uint16_t s_tx_head; /* write */
static uint16_t s_tx_tail; /* read */

static void tx_ring_push(uint8_t b)
{
  uint16_t next = (uint16_t)((s_tx_head + 1u) % CLI_TX_RING);
  if (next != s_tx_tail)
  {
    s_tx_ring[s_tx_head] = b;
    s_tx_head = next;
  }
  /* otherwise full -> drop the byte (prefer losing output over blocking) */
}

static void cli_write_char(EmbeddedCli *cli, char c)
{
  (void)cli;
  tx_ring_push((uint8_t)c);
}

/* Send one contiguous span of the ring (up to its end). If the transport is
   busy, leave the data in the ring and retry on the next cli_process. */
static void tx_flush(void)
{
  if (s_tx == NULL || s_tx_head == s_tx_tail)
  {
    return;
  }
  uint16_t tail = s_tx_tail;
  uint16_t len  = (s_tx_head > tail) ? (uint16_t)(s_tx_head - tail)
                                     : (uint16_t)(CLI_TX_RING - tail);
  if (s_tx(&s_tx_ring[tail], len) == 0)
  {
    s_tx_tail = (uint16_t)((tail + len) % CLI_TX_RING);
  }
}

/* --- Commands (help is built into embedded-cli) ---------------------------- */
static void cmd_version(EmbeddedCli *cli, char *args, void *context)
{
  (void)args;
  (void)context;
  embeddedCliPrint(cli, "bom-stm32node CLI v0.1");
}

void cli_init(cli_tx_fn tx)
{
  s_tx      = tx;
  s_tx_head = 0;
  s_tx_tail = 0;

  EmbeddedCliConfig *cfg = embeddedCliDefaultConfig();
  cfg->cliBuffer         = s_cli_buffer;
  cfg->cliBufferSize     = CLI_STATIC_BYTES;
  cfg->rxBufferSize      = 64;
  cfg->cmdBufferSize     = 64;
  cfg->historyBufferSize = 128;
  cfg->maxBindingCount   = 8;
  cfg->invitation        = "> ";

  s_cli = embeddedCliNew(cfg);
  if (s_cli == NULL)
  {
    /* Static buffer too small. Do NOT trap here: the USB device stack must keep
       being serviced from the main loop, so a CLI failure must not dead-loop.
       cli_process()/cli_feed() are NULL-guarded and simply no-op. */
    return;
  }
  s_cli->writeChar = cli_write_char;

  CliCommandBinding version_binding = {
    .name         = "version",
    .help         = "Print firmware version",
    .tokenizeArgs = false,
    .context      = NULL,
    .binding      = cmd_version,
  };
  embeddedCliAddBinding(s_cli, version_binding);

  embeddedCliProcess(s_cli); /* print the initial prompt */
}

void cli_process(void)
{
  if (s_cli == NULL)
  {
    return;
  }
  embeddedCliProcess(s_cli);
  tx_flush();
}

void cli_feed(const uint8_t *buf, uint32_t len)
{
  if (s_cli == NULL || buf == NULL)
  {
    return;
  }
  for (uint32_t i = 0; i < len; i++)
  {
    embeddedCliReceiveChar(s_cli, (char)buf[i]);
  }
}
