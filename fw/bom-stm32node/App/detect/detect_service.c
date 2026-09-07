/**
 ******************************************************************************
 * @file    detect_service.c
 * @brief   Microphone, pacing and console rendering for the detector
 *          (see detect_service.h).
 ******************************************************************************
 */
#include "detect_service.h"

#include "boomdetect.h"
#include "boomdetect_alarm.h"
#include "boomdetect_selftest.h"
#include "classifier.h"
#include "main.h"    /* HAL_GetTick, DWT */
#include "mic.h"     /* mic_dma_init/start/stop/poll/overrun, PCM_SAMPLES_PER_HALF,
                        PCM_FS_HZ */
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

/* The K-of-N alarm over the window verdicts; re-initialised per run. */
static boomdetect_alarm_t s_alarm;
static const boomdetect_alarm_rule_t s_alarm_rule = {
  .n = DETECT_ALARM_N, .k_on = DETECT_ALARM_K_ON, .k_off = DETECT_ALARM_K_OFF,
};

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
  det_print("DETEND windows=0 drones=0 alarms=0 overrun=0 err=1\r\n");
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
    uint32_t t_ms = boomdetect_frame_to_ms(&s_det, ev->frame_index);
    fmt_milli(dec_str, sizeof(dec_str), ev->rms);
    snprintf(line, sizeof(line), "LVL t=%lu.%03lu rms=%s\r\n",
             (unsigned long)(t_ms / 1000u), (unsigned long)(t_ms % 1000u), dec_str);
    det_print(line);
  }

  if (ev->window.complete)
  {
    /* Timestamped from the frame that CLOSED the window, and the span reported
       alongside. A window is not a fixed 14 frames of wall clock: the gate
       resets accumulation, so it can straddle silence and start arbitrarily far
       from where the decision was actually made. Stamping from start_frame
       alone put the DET line at a time nothing was decided. */
    uint32_t t_ms = boomdetect_frame_to_ms(&s_det, ev->window.end_frame);
    uint32_t span = (ev->window.end_frame - ev->window.start_frame) + 1u;
    fmt_milli(dec_str, sizeof(dec_str), ev->window.decision);
    snprintf(line, sizeof(line), "DET t=%lu.%03lu span=%lu dec=%s %s\r\n",
             (unsigned long)(t_ms / 1000u), (unsigned long)(t_ms % 1000u),
             (unsigned long)span, dec_str,
             ev->window.is_drone ? "DRONE" : "noise");
    det_print(line);

    /* The alarm is reported only when it changes, so a run over a steady drone
       prints one ALM ON and one ALM OFF, not one line per window. */
    if (boomdetect_alarm_push(&s_alarm, ev->window.is_drone))
    {
      snprintf(line, sizeof(line), "ALM t=%lu.%03lu %s hits=%u/%u\r\n",
               (unsigned long)(t_ms / 1000u), (unsigned long)(t_ms % 1000u),
               boomdetect_alarm_on(&s_alarm) ? "ON" : "OFF",
               (unsigned)boomdetect_alarm_hits(&s_alarm), (unsigned)DETECT_ALARM_N);
      det_print(line);
    }
  }
}

void detect_service_run(uint32_t seconds, uint32_t squelch_milli, int32_t thr_milli,
                        uint32_t debug)
{
  static uint8_t s_cyccnt_ready = 0u;
  char           line[80];

  if (!usb_cli_connected())
  {
    return;
  }
  /* Clamped to 1..60, as the header says and as cli.c already enforces. It used
     to return silently on 0, which contradicted both the header's contract and
     the "the trailer always arrives" rule the other error paths follow - a host
     driving this directly would have waited for a DETEND that never came. */
  if (seconds == 0u)
  {
    seconds = 1u;
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

  /* The extractor follows the model, not the board: boomdetect_init() compares
     the model's layout_id against the CONFIGURED extractor and, told nothing,
     configures `stats` (layout 1). Leaving this field out therefore made every
     layout-2/3 model in the registry fail init on hardware while passing the
     host suite, whose tests all resolve the extractor this way (2026-09-07:
     svm_l2, gbt_reg_l2 and cnn_small all answered DETERR). */
  const classifier_t           *model = detect_service_model();
  const boomdetect_extractor_t *ex    = boomdetect_extractor_for_layout(model->layout_id);
  if (ex == NULL)
  {
    char reason[80];
    snprintf(reason, sizeof(reason), "DETERR no extractor for layout %u (model %s)\r\n",
             (unsigned)model->layout_id, model->name);
    det_abort(reason);
    return;
  }

  const boomdetect_config_t cfg = {
    .decimation    = (uint16_t)(PCM_FS_HZ / 16000u), /* 48 kHz in, 16 kHz chain */
    .squelch_milli = squelch_milli,
    .thr_milli     = thr_milli,
    .classifier    = model,
    .extractor     = ex,
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
  if (!boomdetect_alarm_init(&s_alarm, &s_alarm_rule))
  {
    /* The rule is three compile-time constants; this can only fail if someone
       edits them into an inconsistent set, and it should say so rather than
       run with an alarm that never fires. */
    det_abort("DETERR alarm rule invalid\r\n");
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
  snprintf(line, sizeof(line),
           "DETEND windows=%lu drones=%lu alarms=%lu overrun=%u err=%u\r\n",
           (unsigned long)windows, (unsigned long)drones, (unsigned long)s_alarm.onsets,
           ((mic_got && mic_overrun()) || boomdetect_dropped(&s_det) != 0u) ? 1u : 0u,
           (mic_ok && mic_got) ? 0u : 1u);
  det_print(line);
}

/* --- Deterministic self-test (see detect_service.h) ------------------------ */

/* The generator itself lives in fw/common/boomdetect: the fixture it produces
   is checked in there, and a package that ships reference data it cannot
   regenerate has the dependency pointing the wrong way. What is left here is
   the board-specific half - a buffer that must not be a stack frame, CRLF, and
   the USB console. */

/* 6 KB, and static for the same reason s_det is: it was 37 % of the MSPLIM
   budget as a stack frame, in a function that also calls snprintf while USB
   interrupts nest on top - the exact combination that hard-faulted this board
   once already (see the rationale in STM32H563xx_FLASH.ld). */
static int16_t s_dst_block[BOOMDETECT_SELFTEST_BLOCK];

_Static_assert(BOOMDETECT_SELFTEST_BLOCK % 3u == 0u,
               "the selftest block must be a multiple of the decimation factor, "
               "or the kept samples shift and the fixture no longer matches");

/* The package emits bare lines; the console is CRLF, like everything else the
   node prints. */
static void dst_emit(void *ctx, const char *line)
{
  (void)ctx;
  det_print(line);
  det_print("\r\n");
}

void detect_service_selftest(void)
{
  if (!usb_cli_connected())
  {
    return;
  }

  /* Pinned to the deployed model rather than to whatever `model` last selected,
     and resolved explicitly: a NULL from by_name() is not an error to
     boomdetect_init(), it silently means "use the default". Rename or reorder
     the registry and this would quietly measure a different model while the
     reference file sent the reader hunting for a lost -O2. */
  const classifier_t *model = classifier_by_name("mlp_v6");
  if (model == NULL)
  {
    det_print("DSTERR model mlp_v6 is not in this image\r\n");
    det_print("DSTEND frames=0 windows=0 err=1\r\n");
    return;
  }

  /* Leading break: the CLI echo of the command has not been terminated yet at
     this point, so without it DSTBEGIN lands on the same line as the echo and a
     line-oriented reader drops it. */
  det_print("\r\n");
  (void)boomdetect_selftest(&s_det, model, s_dst_block, BOOMDETECT_SELFTEST_BLOCK,
                            dst_emit, NULL);
}
