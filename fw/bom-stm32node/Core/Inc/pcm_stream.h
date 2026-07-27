/**
 ******************************************************************************
 * @file    pcm_stream.h
 * @brief   Stream PCM audio to the host over the USB CDC console.
 *
 * Implements the `stream`/`streamtest` side of the PCM1 wire contract shared
 * with the host tool (fw/apps/stm32node-cli, see its PROTOCOL.md). On request
 * it sends a 16-byte little-endian PCM1 header followed by exactly byte_length
 * bytes of raw int16 mono PCM, then returns to the text prompt. The transfer is
 * length-delimited and rounded up to whole PCM_SAMPLES_PER_HALF blocks.
 ******************************************************************************
 */
#ifndef PCM_STREAM_H
#define PCM_STREAM_H

#include <stdint.h>

/** Maximum stream length in seconds (also enforced by the CLI parser). */
#define PCM_STREAM_MAX_SECONDS 60u

/** Where the streamed samples come from. */
typedef enum
{
  PCM_SRC_MIC,  /**< live microphone via mic.c (needs the SAI/DMA config) */
  PCM_SRC_TONE, /**< synthetic 1 kHz test tone (transport bring-up, no HW) */
} pcm_src_t;

/**
 * @brief Run one PCM stream transfer to the host (blocking).
 *
 * Sends the PCM1 header then pumps whole 1024-sample blocks losslessly over the
 * CDC bulk-IN endpoint. For PCM_SRC_MIC the microphone is started on demand and
 * stopped at the end; a stalled mic (SAI not yet configured) aborts the stream.
 *
 * @param seconds requested duration, clamped to [1, PCM_STREAM_MAX_SECONDS]
 * @param src     sample source
 */
void pcm_stream_run(uint32_t seconds, pcm_src_t src);

#endif /* PCM_STREAM_H */
