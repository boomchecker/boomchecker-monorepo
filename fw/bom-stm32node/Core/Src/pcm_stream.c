/**
 ******************************************************************************
 * @file    pcm_stream.c
 * @brief   PCM streaming to the host over USB CDC (PCM1 wire contract).
 ******************************************************************************
 */
#include "pcm_stream.h"
#include "usb_cli.h"
#include "mic.h"
#include "pdm_pcm.h"
#include "main.h" /* HAL_GetTick, HAL_OK */

#include <math.h>
#include <stdio.h>  /* snprintf (trailer) */
#include <string.h> /* memset (silence padding) */

/* --- PCM1 header (must match stm32node_cli/protocol/spec.py HEADER_FIELDS) -- */
#define PCM_HEADER_SIZE   16u
#define PCM_PROTO_VERSION 1u
#define PCM_CHANNELS      1u
#define PCM_BLOCK_BYTES   (PCM_SAMPLES_PER_HALF * 2u) /* one block = 2048 B */

/* --- Test tone -------------------------------------------------------------- */
#define TONE_HZ    1000.0f
#define TONE_AMPL  16000.0f /* ~ half of int16 full scale */
#define TWO_PI     6.28318530717958647692f

/* Abort a microphone stream if no block arrives within this long (e.g. because
   the SAI/PLL2 config is not in place yet). The test tone never waits. */
#define MIC_BLOCK_TIMEOUT_MS 500u

/* Give up on a stalled bulk-IN send (host stopped reading) instead of spinning
   forever, matching usb_cli_write_blocking's per-transfer stall timeout. */
#define DRAIN_TX_TIMEOUT_MS 2000u

/* Build the 16-byte little-endian PCM1 header in `h`. */
static void pack_header(uint8_t *h, uint32_t byte_length)
{
  const uint32_t sample_rate = PCM_FS_HZ;

  h[0] = 'P';
  h[1] = 'C';
  h[2] = 'M';
  h[3] = '1';
  h[4] = (uint8_t)PCM_PROTO_VERSION;
  h[5] = (uint8_t)PCM_CHANNELS;
  h[6] = 0u; /* reserved */
  h[7] = 0u;
  h[8]  = (uint8_t)(sample_rate);
  h[9]  = (uint8_t)(sample_rate >> 8);
  h[10] = (uint8_t)(sample_rate >> 16);
  h[11] = (uint8_t)(sample_rate >> 24);
  h[12] = (uint8_t)(byte_length);
  h[13] = (uint8_t)(byte_length >> 8);
  h[14] = (uint8_t)(byte_length >> 16);
  h[15] = (uint8_t)(byte_length >> 24);
}

/* Fill one block with a 1 kHz sine; `phase` carries across blocks for continuity. */
static void fill_tone(int16_t *dst, float *phase)
{
  const float step = TWO_PI * TONE_HZ / (float)PCM_FS_HZ;
  for (uint32_t i = 0u; i < PCM_SAMPLES_PER_HALF; i++)
  {
    dst[i] = (int16_t)(TONE_AMPL * sinf(*phase));
    *phase += step;
    if (*phase >= TWO_PI)
    {
      *phase -= TWO_PI;
    }
  }
}

/* Lazily build the DMA linked-list once, then (re)start acquisition. Keeps the
   boot/USB-enumeration path untouched until the first stream is requested.
   Returns true if acquisition started. */
static bool mic_ensure_started(void)
{
  static uint8_t dma_inited = 0u;
  if (!dma_inited)
  {
    mic_dma_init();
    dma_inited = 1u;
  }
  return mic_start() == HAL_OK;
}

/* Send the end-of-stream trailer so the host learns the capture health
   (overrun = gaps, err = mic produced nothing) and gets an explicit end mark. */
static void send_trailer(bool overrun, bool err)
{
  char trailer[32];
  int  n = snprintf(trailer, sizeof(trailer), "PCMEND overrun=%u err=%u\n",
                    overrun ? 1u : 0u, err ? 1u : 0u);
  if (n > 0)
  {
    (void)usb_cli_write_blocking((const uint8_t *)trailer, (uint32_t)n);
  }
}

/* Block until the in-flight send (if any) has finished. Returns false on a
   transport error/disconnect or a host stall. Bounded by a timeout so a host
   that stops draining the bulk-IN endpoint cannot hang the board here (this
   path has no per-transfer timeout of its own, unlike usb_cli_write_blocking).
   On timeout we abort the pipe so the shared write state machine is left clean
   for the next transfer (e.g. the next stream's PCM1 header). */
static bool drain_tx(void)
{
  uint32_t t0 = HAL_GetTick();
  while (usb_cli_write_active())
  {
    if (usb_cli_write_service() < 0)
    {
      return false;
    }
    usb_cli_pump();
    if ((HAL_GetTick() - t0) >= DRAIN_TX_TIMEOUT_MS)
    {
      usb_cli_write_abort();
      return false;
    }
  }
  return true;
}

/* Wait for one processed PCM block. While waiting (~21 ms per half) we service
   the previous block's send, so transmission overlaps acquisition. Returns
   false on timeout (mic not producing data). */
static bool mic_wait_block(int16_t *dst)
{
  size_t   nsamp = 0u;
  uint32_t t0    = HAL_GetTick();
  while (!mic_poll(dst, &nsamp))
  {
    if (usb_cli_write_active())
    {
      usb_cli_write_service();
    }
    else
    {
      usb_cli_pump();
    }
    if ((HAL_GetTick() - t0) >= MIC_BLOCK_TIMEOUT_MS)
    {
      return false;
    }
  }
  return true;
}

void pcm_stream_run(uint32_t seconds, pcm_src_t src)
{
  if (!usb_cli_connected() || seconds == 0u)
  {
    return;
  }
  if (seconds > PCM_STREAM_MAX_SECONDS)
  {
    seconds = PCM_STREAM_MAX_SECONDS;
  }

  /* Round up to whole blocks; byte_length in the header is authoritative. */
  const uint32_t blocks =
      (seconds * PCM_FS_HZ + PCM_SAMPLES_PER_HALF - 1u) / PCM_SAMPLES_PER_HALF;
  const uint32_t byte_length = blocks * PCM_BLOCK_BYTES;

  uint8_t header[PCM_HEADER_SIZE];
  pack_header(header, byte_length);

  /* Start from a clean transmit state machine. If a previous stream was
     interrupted (host closed / Ctrl-C), the shared CDC write state can be left
     mid-transfer; resetting it here guarantees the header below actually goes
     out instead of re-driving a stale transfer (which showed up as an
     intermittent "magic not found" on the host). */
  usb_cli_write_abort();

  /* Flush the command echo before the binary so the host only skips leading
     text (its magic resync), never interleaved bytes. */
  usb_cli_flush_tx();
  if (usb_cli_write_blocking(header, PCM_HEADER_SIZE) != 0)
  {
    return;
  }

  /* Two blocks ping-pong: one is transmitted over USB while the next is being
     acquired/generated. Kept off the stack (2 x 2 KB). */
  static int16_t block_a[PCM_SAMPLES_PER_HALF];
  static int16_t block_b[PCM_SAMPLES_PER_HALF];
  int16_t *const bufs[2] = {block_a, block_b};
  float          phase   = 0.0f;
  bool           mic_live = false; /* mic still delivering blocks */
  bool           mic_got  = false; /* mic delivered at least one block */
  bool           tx_ok    = true;

  if (src == PCM_SRC_MIC)
  {
    mic_live = mic_ensure_started();
  }

  for (uint32_t i = 0u; i < blocks; i++)
  {
    int16_t *cur = bufs[i & 1u];

    /* Fill `cur`. For the mic this waits ~21 ms, during which the previous
       block's send runs to completion in the background (see mic_wait_block). */
    if (src == PCM_SRC_TONE)
    {
      fill_tone(cur, &phase);
    }
    else if (mic_live && mic_wait_block(cur))
    {
      mic_got = true;
    }
    else
    {
      /* Mic not producing: pad silence so the host still receives the full
         byte_length (never hangs). Stop waiting to keep the rest quick. */
      mic_live = false;
      memset(cur, 0, PCM_BLOCK_BYTES);
    }

    /* The previous send used the *other* buffer; make sure it released it,
       then hand off `cur` and go acquire the next block while it transmits. */
    if (!drain_tx() || usb_cli_write_start((const uint8_t *)cur, PCM_BLOCK_BYTES) != 0)
    {
      tx_ok = false;
      break; /* transport gone: host will time out */
    }
  }

  if (tx_ok)
  {
    drain_tx(); /* flush the final block */
  }

  if (src == PCM_SRC_MIC)
  {
    mic_stop();
  }

  /* Explicit end-of-stream confirmation + capture health. */
  if (tx_ok)
  {
    send_trailer((src == PCM_SRC_MIC) && mic_overrun(),
                 (src == PCM_SRC_MIC) && !mic_got);
  }
}
