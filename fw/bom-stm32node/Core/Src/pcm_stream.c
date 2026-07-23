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
#include "main.h" /* HAL_GetTick */

#include <math.h>

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
   boot/USB-enumeration path untouched until the first stream is requested. */
static void mic_ensure_started(void)
{
  static uint8_t dma_inited = 0u;
  if (!dma_inited)
  {
    mic_dma_init();
    dma_inited = 1u;
  }
  (void)mic_start();
}

/* Wait for one processed PCM block, keeping the USB stack alive. Returns false
   on timeout (mic not producing data). */
static bool mic_wait_block(int16_t *dst)
{
  size_t   nsamp = 0u;
  uint32_t t0    = HAL_GetTick();
  while (!mic_poll(dst, &nsamp))
  {
    usb_cli_pump();
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

  /* Flush the command echo before the binary so the host only skips leading
     text (its magic resync), never interleaved bytes. */
  usb_cli_flush_tx();
  if (usb_cli_write_blocking(header, PCM_HEADER_SIZE) != 0)
  {
    return;
  }

  static int16_t block[PCM_SAMPLES_PER_HALF]; /* kept off the stack */
  float          phase = 0.0f;

  if (src == PCM_SRC_MIC)
  {
    mic_ensure_started();
  }

  for (uint32_t i = 0u; i < blocks; i++)
  {
    if (src == PCM_SRC_TONE)
    {
      fill_tone(block, &phase);
    }
    else if (!mic_wait_block(block))
    {
      break; /* mic stalled: abort (host will time out on the short read) */
    }

    if (usb_cli_write_blocking((const uint8_t *)block, PCM_BLOCK_BYTES) != 0)
    {
      break;
    }
  }

  if (src == PCM_SRC_MIC)
  {
    mic_stop();
  }
}
