/**
 ******************************************************************************
 * @file    detect_service.c
 * @brief   Microphone, pacing and console rendering for the detector
 *          (see detect_service.h).
 ******************************************************************************
 */
#include "detect_service.h"

#include "boomdetect.h"
#include "main.h"    /* HAL_GetTick, DWT */
#include "mic.h"     /* mic_dma_init/start/stop/poll, PCM_SAMPLES_PER_HALF */
#include "usb_cli.h" /* usb_cli_pump / connected / write_blocking */

#include <stdio.h>
#include <string.h>

#define DET_MAX_SECONDS 60u

/* Report the input level once a second (31 frames) so the operator can aim the
   source or the volume even when the squelch keeps windows from completing. */
#define DET_LVL_EVERY 31u

/* Abort waiting for a mic block after this long (mic/SAI not producing). */
#define DET_MIC_TIMEOUT_MS 500u

/* ~21 KB, so static rather than a stack frame. */
static boomdetect_t s_det;
static int16_t      s_pcm[PCM_SAMPLES_PER_HALF];

/* Selected model. NULL means "whatever the registry calls default", resolved
   late so this file does not need an initialiser that runs before main. */
static const classifier_t *s_model;

const classifier_t *detect_service_model(void)
{
  return (s_model != NULL) ? s_model : classifier_default();
}

bool detect_service_set_model(const char *name)
{
  const classifier_t *m = classifier_by_name(name);
  if (m == NULL)
  {
    return false;
  }
  s_model = m;
  return true;
}

/* Cycle-accurate timing of the two hot operations (250 MHz -> 250 cyc/us).
   Cheap enough to keep always-on; reported on the debug breadcrumbs. */
static uint32_t s_half_us; /* last mic_poll, including the PDM conversion */
static uint32_t s_step_us; /* last boomdetect_step, i.e. one frame of DSP */

static void cyccnt_init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0u;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* Format milli-units as "+d.ddd" without float printf (newlib-nano has none). */
static void fmt_milli(char *dst, size_t len, float value)
{
  int32_t  milli = (int32_t)(value * 1000.0f + ((value >= 0.0f) ? 0.5f : -0.5f));
  uint32_t mag   = (milli < 0) ? (uint32_t)(-milli) : (uint32_t)milli;
  snprintf(dst, len, "%c%lu.%03lu", (milli < 0) ? '-' : '+',
           (unsigned long)(mag / 1000u), (unsigned long)(mag % 1000u));
}

static void det_print(const char *line)
{
  (void)usb_cli_write_blocking((const uint8_t *)line, (uint32_t)strlen(line));
}

/* Error exit. The console contract promises a DETEND trailer for every run, so
   an aborted start still ends with one instead of leaving a host waiting. */
static void det_abort(const char *reason)
{
  det_print(reason);
  det_print("DETEND windows=0 drones=0 overrun=0 err=1\r\n");
}

/* Wait for one processed PCM block, keeping the USB device serviced. The pump
   runs at least once per call so sustained processing (halves already queued)
   cannot starve the USB stack. */
static bool det_wait_block(size_t *nsamp)
{
  uint32_t t0 = HAL_GetTick();
  usb_cli_pump();
  for (;;)
  {
    uint32_t c0 = DWT->CYCCNT;
    if (mic_poll(s_pcm, nsamp))
    {
      s_half_us = (DWT->CYCCNT - c0) / 250u;
      return true;
    }
    usb_cli_pump();
    if (!usb_cli_connected() || (HAL_GetTick() - t0) >= DET_MIC_TIMEOUT_MS)
    {
      return false;
    }
  }
}

/* One frame of DSP, timed. Returns what boomdetect_step did. */
static bool det_step_timed(boomdetect_event_t *ev)
{
  const uint32_t c0 = DWT->CYCCNT;
  const bool     got = boomdetect_step(&s_det, ev);
  if (got)
  {
    s_step_us = (DWT->CYCCNT - c0) / 250u;
  }
  return got;
}

static void det_report(const boomdetect_event_t *ev, uint32_t debug)
{
  char line[80];
  char dec_str[16];

  if (debug)
  {
    snprintf(line, sizeof(line), "F=%lu a=%lu r=%lu h=%lu m=%lu\r\n",
             (unsigned long)ev->frame_index, (unsigned long)ev->accum,
             (unsigned long)(ev->rms * 1000.0f), (unsigned long)s_half_us,
             (unsigned long)s_step_us);
    det_print(line);
  }

  if ((ev->frame_index % DET_LVL_EVERY) == 0u)
  {
    uint32_t t_ms = (ev->frame_index * BOOMDETECT_HOP) / 16u;
    fmt_milli(dec_str, sizeof(dec_str), ev->rms);
    snprintf(line, sizeof(line), "LVL t=%lu.%03lu rms=%s\r\n",
             (unsigned long)(t_ms / 1000u), (unsigned long)(t_ms % 1000u), dec_str);
    det_print(line);
  }

  if (ev->window_complete)
  {
    uint32_t t_ms = (ev->window_start_frame * BOOMDETECT_HOP) / 16u; /* /16000*1000 */
    fmt_milli(dec_str, sizeof(dec_str), ev->decision);
    snprintf(line, sizeof(line), "DET t=%lu.%03lu dec=%s %s\r\n",
             (unsigned long)(t_ms / 1000u), (unsigned long)(t_ms % 1000u), dec_str,
             ev->is_drone ? "DRONE" : "noise");
    det_print(line);
  }
}

void detect_service_run(uint32_t seconds, uint32_t squelch_milli, int32_t thr_milli,
                        uint32_t debug)
{
  static uint8_t s_cyccnt_ready = 0u;
  char           line[80];

  if (!usb_cli_connected() || seconds == 0u)
  {
    return;
  }
  if (seconds > DET_MAX_SECONDS)
  {
    seconds = DET_MAX_SECONDS;
  }

  if (!s_cyccnt_ready)
  {
    cyccnt_init();
    s_cyccnt_ready = 1u;
  }

  const boomdetect_config_t cfg = {
    .decimation    = (uint16_t)(PCM_FS_HZ / 16000u), /* 48 kHz in, 16 kHz chain */
    .squelch_milli = squelch_milli,
    .thr_milli     = thr_milli,
    .classifier    = detect_service_model(),
  };
  if (!boomdetect_init(&s_det, &cfg))
  {
    /* Four distinct causes, and three have nothing to do with the MFCC: bad
       arguments, decimation 0, a model the registry rejected (no decide
       function, a foreign layout_id, or a slice past the feature vector), and
       only then an MFCC table failure. Reporting them all as "mfcc init
       failed" sent the reader to the wrong half of the system. */
    char reason[80];
    snprintf(reason, sizeof(reason), "DETERR init failed for model %s\r\n",
             detect_service_model()->name);
    det_abort(reason);
    return;
  }

  /* NOTE: do NOT call usb_cli_flush_tx() here. Flushing the console ring from
     inside the CLI binding wedges the CDC write state machine (observed on
     2026-08-07: builds with the flush produced zero output; without it the
     DET prints work - usb_cli_write_blocking finishes any staged chunk
     itself). The command echo simply goes out after the run, with the prompt. */

  mic_dma_init(); /* no-op when already built (shared with pcm_stream) */
  if (mic_start() != 0)
  {
    det_abort("DETERR mic start failed\r\n");
    return;
  }

  bool mic_got = false;
  bool mic_ok  = true;

  const uint32_t halves =
      (seconds * PCM_FS_HZ + PCM_SAMPLES_PER_HALF - 1u) / PCM_SAMPLES_PER_HALF;

  for (uint32_t h = 0u; h < halves; h++)
  {
    size_t nsamp = 0u;
    if (!det_wait_block(&nsamp))
    {
      mic_ok = false;
      break;
    }
    mic_got = true;

    boomdetect_push(&s_det, s_pcm, nsamp);

    /* At most one frame per mic block, which is why this is an `if` and not a
       `while`: two MFCCs in one iteration overran the 21.33 ms budget and
       starved the USB stack. Average inflow is 341 samples per block against
       512 consumed per frame, so the FIFO cannot grow unbounded regardless. */
    usb_cli_pump();
    boomdetect_event_t ev;
    if (det_step_timed(&ev))
    {
      det_report(&ev, debug);
    }
  }

  mic_stop();

  uint32_t windows = 0u, drones = 0u;
  boomdetect_counts(&s_det, &windows, &drones);
  snprintf(line, sizeof(line), "DETEND windows=%lu drones=%lu overrun=%u err=%u\r\n",
           (unsigned long)windows, (unsigned long)drones,
           ((mic_got && mic_overrun()) || boomdetect_dropped(&s_det) != 0u) ? 1u : 0u,
           (mic_ok && mic_got) ? 0u : 1u);
  det_print(line);
}

/* --- Deterministic self-test (see detect_service.h) ------------------------ */

/* 48 kHz input long enough for THREE full windows (42 frames): 1024 + 41*512 at
   16 kHz, tripled upstream. One window would leave the accumulator reset
   between windows untested, which is exactly the kind of state bug a move can
   introduce. */
#define DST_INPUT_LEN   66048u
#define DST_LCG_SEED    1u
#define DST_MFCC_FRAMES 3u /* how many frames' coefficients to print */

/* Fed in blocks rather than all at once: the FIFO holds 4096 samples and the
   signal decimates to 22016, so one push would overflow it and start dropping.
   3072 is a multiple of the decimation factor, so the phase stays at zero and
   the kept samples are the same ones a continuous stream would give. */
#define DST_BLOCK 3072u

/* 6 KB, and static for the same reason s_det is: it was 37 % of the MSPLIM
   budget as a stack frame, in a function that also calls snprintf while USB
   interrupts nest on top - the exact combination that hard-faulted this board
   once already (see the rationale in STM32H563xx_FLASH.ld). */
static int16_t s_dst_block[DST_BLOCK];

_Static_assert(DST_BLOCK % 3u == 0u,
               "DST_BLOCK must be a multiple of the decimation factor, or the "
               "kept samples shift and the fixture no longer matches");

/* One sample of the reference signal. Integer LCG, so every platform produces
   the same bits - a sinf() table would differ in the last place between the
   Cortex-M33 and an x86 host and would defeat the whole point. Amplitude is
   quartered to sit around -12 dBFS instead of slamming the CIC at full scale. */
static int16_t dst_sample(uint32_t *state)
{
  *state = (*state * 1103515245u) + 12345u;
  int32_t v = (int32_t)((*state >> 16) & 0xFFFFu) - 32768; /* -32768..32767 */
  return (int16_t)(v / 4);
}

/* Print a float as its raw bit pattern. Decimal would need %f (absent from
   newlib-nano, hence fmt_milli) and would round away exactly the differences
   this test exists to catch. */
static uint32_t dst_bits(float v)
{
  uint32_t u;
  memcpy(&u, &v, sizeof(u));
  return u;
}

static void dst_print_vec(char *line, size_t len, const char *prefix, const float *v,
                          uint32_t n)
{
  int w = snprintf(line, len, "%s", prefix);
  for (uint32_t c = 0u; c < n && w > 0 && (size_t)w < len; c++)
  {
    w += snprintf(line + w, len - (size_t)w, " %08lX", (unsigned long)dst_bits(v[c]));
  }
  det_print(line);
  det_print("\r\n");
}

void detect_service_selftest(void)
{
  static const char *stat[4] = { "mean", "std ", "dmea", "cmax" };
  char     line[160];
  char     prefix[24];
  uint32_t state = DST_LCG_SEED;
  uint32_t fnv = 2166136261u;
  uint32_t windows = 0u;

  if (!usb_cli_connected())
  {
    return;
  }

  /* No squelch: the gate is a policy knob, and letting it drop frames would
     make the fixture depend on the signal's level rather than on the arithmetic
     it is meant to pin down. */
  /* Pinned to the deployed model so the fixture does not shift when someone
     leaves another one selected - and resolved explicitly, because a NULL from
     by_name() is not an error to boomdetect_init(), it silently means "use the
     default". Rename or reorder the registry and this would quietly measure a
     different model while the reference file sends the reader hunting for a
     lost -O2 or a flipped LOOPUNROLL. */
  const classifier_t *model = classifier_by_name("mlp_v6");
  if (model == NULL)
  {
    det_print("DSTERR model mlp_v6 is not in this image\r\n");
    det_print("DSTEND frames=0 windows=0 err=1\r\n");
    return;
  }

  const boomdetect_config_t cfg = {
    .decimation    = (uint16_t)(PCM_FS_HZ / 16000u),
    .squelch_milli = 0u,
    .thr_milli     = model->default_thr_milli,
    .classifier    = model,
  };
  if (!boomdetect_init(&s_det, &cfg))
  {
    det_print("DSTERR detector init failed\r\n");
    det_print("DSTEND frames=0 windows=0 err=1\r\n");
    return;
  }

  /* Leading break: the CLI echo of the command has not been terminated yet at
     this point, so without it DSTBEGIN lands on the same line as the echo and a
     line-oriented reader drops it. */
  det_print("\r\nDSTBEGIN\r\n");

  for (uint32_t off = 0u; off < DST_INPUT_LEN; off += DST_BLOCK)
  {
    /* Clamp rather than assume DST_BLOCK divides DST_INPUT_LEN. It does not
       (66048 / 3072 = 21.5), and running the loop one block long generated
       1536 samples too many - which the fixture caught as a 43rd frame and a
       different checksum. A short final block is fine: boomdetect_push carries
       the decimation phase across calls. */
    const uint32_t n = (DST_INPUT_LEN - off < DST_BLOCK) ? (DST_INPUT_LEN - off)
                                                         : DST_BLOCK;
    for (uint32_t i = 0u; i < n; i++)
    {
      const int16_t x = dst_sample(&state);
      s_dst_block[i] = x;
      /* FNV-1a over the generated samples: proves both sides scored the same
         input before comparing anything downstream of it. */
      fnv = (fnv ^ (uint32_t)((uint16_t)x & 0xFFu)) * 16777619u;
      fnv = (fnv ^ (uint32_t)(((uint16_t)x >> 8) & 0xFFu)) * 16777619u;
    }
    boomdetect_push(&s_det, s_dst_block, n);

    /* Drain fully here. detect_service_run caps this at one frame per block to
       keep USB fed; that is a pacing constraint, not arithmetic, so draining
       yields the same frames in the same order. */
    boomdetect_event_t ev;
    while (boomdetect_step(&s_det, &ev))
    {
      /* NULL for a squelched frame, which cannot happen here because this
         runs with squelch 0 - but that is an accident of the config above, not
         a property of the accessor, and dereferencing it would be a hard fault
         on the board rather than a wrong number. */
      const float *mf = boomdetect_last_mfcc(&s_det);
      if (ev.frame_index < DST_MFCC_FRAMES && mf != NULL)
      {
        snprintf(prefix, sizeof(prefix), "DSTMFCC f=%lu",
                 (unsigned long)ev.frame_index);
        dst_print_vec(line, sizeof(line), prefix, mf, NUM_MFCC_COEFFS);
      }

      if (ev.window_complete)
      {
        /* Every window, not just the first: the aggregate is rebuilt from a
           reset accumulator each time, so a state bug shows up in window 1
           while window 0 still looks perfect. */
        const float *f = boomdetect_last_features(&s_det);
        for (uint32_t g = 0u; g < 4u; g++)
        {
          snprintf(prefix, sizeof(prefix), "DSTFEAT w=%lu %s", (unsigned long)windows,
                   stat[g]);
          dst_print_vec(line, sizeof(line), prefix, f + g * NUM_MFCC_COEFFS,
                        NUM_MFCC_COEFFS);
        }
        snprintf(line, sizeof(line), "DSTDEC w=%lu logit=%08lX\r\n",
                 (unsigned long)windows, (unsigned long)dst_bits(ev.decision));
        det_print(line);
        windows++;
      }
    }
  }

  uint32_t win = 0u, dro = 0u;
  boomdetect_counts(&s_det, &win, &dro);
  snprintf(line, sizeof(line), "DSTSIG n=%lu seed=%lu fnv=%08lX\r\n",
           (unsigned long)DST_INPUT_LEN, (unsigned long)DST_LCG_SEED,
           (unsigned long)fnv);
  det_print(line);
  snprintf(line, sizeof(line), "DSTEND frames=%lu windows=%lu err=0\r\n",
           (unsigned long)s_det.frame_index, (unsigned long)win);
  det_print(line);
}
