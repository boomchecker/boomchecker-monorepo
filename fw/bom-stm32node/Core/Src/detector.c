/**
 ******************************************************************************
 * @file    detector.c
 * @brief   On-device drone detection pipeline (see detector.h).
 *
 * Mirrors the PC reference implementation (drony validate_real_wav.py /
 * audio_sai_handler.c semantics): per-frame RMS squelch resets accumulation,
 * every run of 14 accepted frames is aggregated to mean+std features and
 * classified. The decimation is a plain pick-every-3rd with phase carried
 * across blocks - no extra FIR (the 101-tap 8 kHz low-pass in pdm_pcm.c is
 * the anti-alias filter; a short FIR here measurably corrupts the features).
 ******************************************************************************
 */
#include "detector.h"
#include "mic.h"            /* mic_dma_init/start/stop/poll, PCM_SAMPLES_PER_HALF */
#include "mfcc_processor.h" /* mfcc_init / mfcc_process (CMSIS-DSP arm_mfcc_f32) */
#include "svm_classifier.h" /* svm_get_decision_value (weights: svm_model_data.h) */
#include "dsp_config.h"     /* WINDOW_SIZE, NUM_MFCC_COEFFS */
#include "usb_cli.h"        /* usb_cli_pump / connected / write_blocking */
#include "main.h"           /* HAL_GetTick */

#include <math.h>   /* sqrtf */
#include <stdio.h>  /* snprintf */
#include <string.h> /* memset */

/* Fixed pipeline geometry; squelch/threshold arrive as arguments. */
#define DET_ACCUM_FRAMES    14u
#define DET_HOP             512u
#define DET_MAX_SECONDS     60u
/* Report the input level once a second (31 frames) so the operator can aim
   the source/volume even when the squelch keeps windows from completing. */
#define DET_LVL_EVERY       31u

/* 16 kHz sample FIFO: must hold one window plus slack for jitter. */
#define DET_RING_LEN        4096u

/* Abort waiting for a mic block after this long (mic/SAI not producing). */
#define DET_MIC_TIMEOUT_MS  500u

static float det_ring[DET_RING_LEN];   /* 16 kHz float FIFO            (16 KB) */
static float det_frame[WINDOW_SIZE];   /* contiguous frame for MFCC     (4 KB) */
static float det_mfccs[DET_ACCUM_FRAMES * NUM_MFCC_COEFFS];
static int16_t det_pcm[PCM_SAMPLES_PER_HALF];                        /* (2 KB) */

/* Cycle-accurate timing of the two hot operations (250 MHz -> 250 cyc/us).
   Cheap enough to keep always-on; reported on the debug breadcrumbs. */
static uint32_t det_half_us; /* last mic_poll incl. pdm_pcm conversion */
static uint32_t det_mfcc_us; /* last mfcc_process */

static void cyccnt_init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0u;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* Format milli-units as "+d.ddd" without float printf (newlib-nano). */
static void fmt_milli(char *dst, size_t len, float value)
{
  int32_t milli = (int32_t)(value * 1000.0f + ((value >= 0.0f) ? 0.5f : -0.5f));
  uint32_t mag  = (milli < 0) ? (uint32_t)(-milli) : (uint32_t)milli;
  snprintf(dst, len, "%c%lu.%03lu", (milli < 0) ? '-' : '+',
           (unsigned long)(mag / 1000u), (unsigned long)(mag % 1000u));
}

static void det_print(const char *line)
{
  (void)usb_cli_write_blocking((const uint8_t *)line, (uint32_t)strlen(line));
}

/* Wait for one processed PCM block, keeping the USB device serviced. The pump
   runs at least once per call so sustained processing (halves already queued)
   cannot starve the USB stack. */
static bool det_wait_block(void)
{
  size_t   nsamp = 0u;
  uint32_t t0    = HAL_GetTick();
  usb_cli_pump();
  for (;;)
  {
    uint32_t c0 = DWT->CYCCNT;
    if (mic_poll(det_pcm, &nsamp))
    {
      det_half_us = (DWT->CYCCNT - c0) / 250u;
      return true;
    }
    usb_cli_pump();
    if (!usb_cli_connected() || (HAL_GetTick() - t0) >= DET_MIC_TIMEOUT_MS)
    {
      return false;
    }
  }
}

/* Aggregate 14 MFCC frames to [mean_c0..12, std_c0..12] (population std,
   matching np.std and the training pipeline). */
static void det_aggregate(const float *frames, float *out)
{
  for (uint32_t c = 0u; c < NUM_MFCC_COEFFS; c++)
  {
    float sum = 0.0f;
    for (uint32_t f = 0u; f < DET_ACCUM_FRAMES; f++)
    {
      sum += frames[f * NUM_MFCC_COEFFS + c];
    }
    float mean = sum / (float)DET_ACCUM_FRAMES;
    out[c] = mean;

    float sq = 0.0f;
    for (uint32_t f = 0u; f < DET_ACCUM_FRAMES; f++)
    {
      float d = frames[f * NUM_MFCC_COEFFS + c] - mean;
      sq += d * d;
    }
    out[c + NUM_MFCC_COEFFS] = sqrtf(sq / (float)DET_ACCUM_FRAMES);
  }
}

void detector_run(uint32_t seconds, uint32_t squelch_milli, int32_t thr_milli,
                  uint32_t debug)
{
  static uint8_t s_dsp_ready = 0u;
  char           line[64];
  char           dec_str[16];

  const float squelch   = (float)squelch_milli / 1000.0f;
  const float threshold = (float)thr_milli / 1000.0f;

  if (!usb_cli_connected() || seconds == 0u)
  {
    return;
  }
  if (seconds > DET_MAX_SECONDS)
  {
    seconds = DET_MAX_SECONDS;
  }

  if (!s_dsp_ready)
  {
    if (mfcc_init() != ARM_MATH_SUCCESS)
    {
      det_print("DETERR mfcc init failed\n");
      return;
    }
    svm_classifier_init();
    cyccnt_init();
    s_dsp_ready = 1u;
  }

  /* NOTE: do NOT call usb_cli_flush_tx() here. Flushing the console ring from
     inside the CLI binding wedges the CDC write state machine (observed on
     2026-08-07: builds with the flush produced zero output; without it the
     DET prints work - usb_cli_write_blocking finishes any staged chunk
     itself). The command echo simply goes out after the run, with the prompt. */

  mic_dma_init(); /* no-op when already built (shared with pcm_stream) */
  if (mic_start() != 0)
  {
    det_print("DETERR mic start failed\n");
    return;
  }

  /* Ring/decimator/accumulator state is per-run. */
  uint32_t w_idx = 0u, r_idx = 0u, avail = 0u;
  uint32_t dec_phase = 0u;          /* index of the next 48 kHz sample to keep  */
  uint32_t accum = 0u;              /* accepted frames in the current window    */
  uint32_t frame_idx = 0u;          /* absolute 16 kHz frame counter            */
  uint32_t win_start_frame = 0u;
  uint32_t windows = 0u, drones = 0u;
  bool     mic_got = false;
  bool     mic_ok  = true;

  const uint32_t halves = (seconds * PCM_FS_HZ + PCM_SAMPLES_PER_HALF - 1u)
                          / PCM_SAMPLES_PER_HALF;

  for (uint32_t h = 0u; h < halves; h++)
  {
    if (!det_wait_block())
    {
      mic_ok = false;
      break;
    }
    mic_got = true;

    /* Decimate by 3 straight into the FIFO, phase carried across blocks
       (PCM_SAMPLES_PER_HALF is not a multiple of 3). */
    uint32_t i = dec_phase;
    for (; i < PCM_SAMPLES_PER_HALF; i += 3u)
    {
      det_ring[w_idx] = (float)det_pcm[i] * (1.0f / 32768.0f);
      w_idx = (w_idx + 1u) % DET_RING_LEN;
      avail++;
    }
    dec_phase = i - PCM_SAMPLES_PER_HALF;

    /* Process AT MOST ONE frame per mic half. Average inflow is 341 samples
       per half vs 512 consumed per frame, so the FIFO cannot grow unbounded;
       capping to one frame spreads MFCC bursts across halves and keeps every
       loop iteration under the 21.33 ms real-time budget (a burst of two
       frames in one iteration was enough to starve the USB stack). */
    usb_cli_pump();
    if (avail >= WINDOW_SIZE)
    {
      uint32_t idx = r_idx;
      for (uint32_t k = 0u; k < WINDOW_SIZE; k++)
      {
        det_frame[k] = det_ring[idx];
        idx = (idx + 1u) % DET_RING_LEN;
      }

      float rms = 0.0f;
      arm_rms_f32(det_frame, WINDOW_SIZE, &rms);

      if (debug)
      {
        snprintf(line, sizeof(line), "F=%lu a=%lu r=%lu h=%lu m=%lu\n",
                 (unsigned long)frame_idx, (unsigned long)accum,
                 (unsigned long)(rms * 1000.0f),
                 (unsigned long)det_half_us, (unsigned long)det_mfcc_us);
        det_print(line);
      }

      if ((frame_idx % DET_LVL_EVERY) == 0u)
      {
        uint32_t t_ms = (frame_idx * DET_HOP) / 16u;
        fmt_milli(dec_str, sizeof(dec_str), rms);
        snprintf(line, sizeof(line), "LVL t=%lu.%03lu rms=%s\n",
                 (unsigned long)(t_ms / 1000u), (unsigned long)(t_ms % 1000u),
                 dec_str);
        det_print(line);
      }

      if (rms < squelch)
      {
        accum = 0u; /* too quiet: reset the window accumulation */
      }
      else
      {
        if (accum == 0u)
        {
          win_start_frame = frame_idx;
        }
        /* arm_mfcc_f32 destroys its input; det_frame is a scratch copy. */
        uint32_t c0 = DWT->CYCCNT;
        mfcc_process(det_frame, &det_mfccs[accum * NUM_MFCC_COEFFS]);
        det_mfcc_us = (DWT->CYCCNT - c0) / 250u;
        accum++;

        if (accum >= DET_ACCUM_FRAMES)
        {
          float features[2u * NUM_MFCC_COEFFS];
          det_aggregate(det_mfccs, features);
          float decision = svm_get_decision_value(features);
          int   is_drone = (decision >= threshold);

          uint32_t t_ms = (win_start_frame * DET_HOP) / 16u; /* /16000*1000 */
          fmt_milli(dec_str, sizeof(dec_str), decision);
          snprintf(line, sizeof(line), "DET t=%lu.%03lu dec=%s %s\n",
                   (unsigned long)(t_ms / 1000u), (unsigned long)(t_ms % 1000u),
                   dec_str, is_drone ? "DRONE" : "noise");
          det_print(line);

          windows++;
          if (is_drone)
          {
            drones++;
          }
          accum = 0u;
        }
      }

      r_idx = (r_idx + DET_HOP) % DET_RING_LEN;
      avail -= DET_HOP;
      frame_idx++;
    }
  }

  mic_stop();

  snprintf(line, sizeof(line), "DETEND windows=%lu drones=%lu overrun=%u err=%u\n",
           (unsigned long)windows, (unsigned long)drones,
           (mic_got && mic_overrun()) ? 1u : 0u,
           (mic_ok && mic_got) ? 0u : 1u);
  det_print(line);
}
