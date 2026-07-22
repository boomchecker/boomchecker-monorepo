/**
 ******************************************************************************
 * @file    cli.c
 * @brief   Konzole nad embedded-cli s vystupem pres transportni callback.
 ******************************************************************************
 */
#include "cli.h"
#include "embedded_cli.h"
#include "main.h"   /* Error_Handler */

/* Staticka alokace CLI (bez malloc). 1 kB pokryje rx/cmd/history nize + bindingy. */
#define CLI_STATIC_BYTES  1024u
#define CLI_TX_RING       512u

static EmbeddedCli *s_cli;
static CLI_UINT     s_cli_buffer[BYTES_TO_CLI_UINTS(CLI_STATIC_BYTES)];
static cli_tx_fn    s_tx;

/* TX kruhovy buffer: writeChar plni po znaku, cli_process davkove flushuje. */
static uint8_t  s_tx_ring[CLI_TX_RING];
static uint16_t s_tx_head; /* zapis */
static uint16_t s_tx_tail; /* cteni */

static void tx_ring_push(uint8_t b)
{
  uint16_t next = (uint16_t)((s_tx_head + 1u) % CLI_TX_RING);
  if (next != s_tx_tail)
  {
    s_tx_ring[s_tx_head] = b;
    s_tx_head = next;
  }
  /* jinak plno -> znak se zahodi (radeji ztrata vypisu nez blokovani) */
}

static void cli_write_char(EmbeddedCli *cli, char c)
{
  (void)cli;
  tx_ring_push((uint8_t)c);
}

/* Odesle jednu souvislou cast ringu (do konce bufferu). Pri busy transportu
   necha data v ringu a zkusi to znovu pristi cli_process. */
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

/* --- Prikazy (help je vestaveny v embedded-cli) ---------------------------- */
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
    Error_Handler(); /* staticky buffer je maly - zvednout CLI_STATIC_BYTES */
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

  embeddedCliProcess(s_cli); /* vypise uvodni pozvanku */
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
