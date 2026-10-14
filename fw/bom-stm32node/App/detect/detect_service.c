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
#include "gps.h"     /* gps_track_*, gps_last_fix: the position on ALARM lines */
#include "main.h"    /* HAL_GetTick, DWT */
#include "mic.h"     /* mic_dma_init/start/stop/poll/overrun, PCM_SAMPLES_PER_HALF,
                        PCM_FS_HZ */
#include "usb_cli.h" /* usb_cli_pump / connected / write_blocking */

#include <stdio.h>
#include <string.h>

/* Report the input level once a second (31 frames) so the operator can aim the
   source or the volume even when the squelch keeps windows from completing. */
#define DET_LVL_EVERY 31u

/* Abort waiting for a mic block after this long (mic/SAI not producing). */
#define DET_MIC_TIMEOUT_MS 500u

/* ~51 KB, so static rather than a stack frame. */
static boomdetect_t s_det;
static int16_t      s_pcm[PCM_SAMPLES_PER_HALF];

/* The alarm over the window decisions; rule and state re-initialised per run. */
static boomdetect_alarm_t s_alarm;
static const boomdetect_alarm_rule_t s_alarm_default = {
  .n = DETECT_ALARM_N, .k_on = DETECT_ALARM_K_ON, .k_off = DETECT_ALARM_K_OFF,
  .mode = BOOMDETECT_ALARM_VOTE,
};
static float s_thr; /* this run's decision threshold, for the relative decision */

/* When this run first called a window DRONE and first raised the alarm, in ms
   since the run started; UINT32_MAX until it happens. Both go into DETEND so a
   field log needs nothing but the trailer: how long the drone had to be
   audible before the first window and the first alarm said so. */
static uint32_t s_first_drone_ms;
static uint32_t s_first_alarm_ms;

/* "<s>.<mmm>" for a time that happened, "-" for one that did not. */
static void fmt_when(char *out, size_t len, uint32_t ms)
{
  if (ms == UINT32_MAX)
  {
    snprintf(out, len, "-");
  }
  else
  {
    snprintf(out, len, "%lu.%03lu", (unsigned long)(ms / 1000u), (unsigned long)(ms % 1000u));
  }
}

/* "<k>of<n>" or "mean<n>", nothing else: the token has to be the whole string. */
static bool parse_u8(const char *s, const char **end, uint8_t *out)
{
  unsigned long v = 0u;
  const char   *p = s;
  while (*p >= '0' && *p <= '9')
  {
    v = v * 10u + (unsigned long)(*p - '0');
    if (v > 255u)
    {
      return false;
    }
    p++;
  }
  if (p == s)
  {
    return false;
  }
  *end = p;
  *out = (uint8_t)v;
  return true;
}

bool detect_service_parse_rule(const char *token, boomdetect_alarm_rule_t *out)
{
  boomdetect_alarm_rule_t rule = { 0 };
  const char             *p    = token;
  if (token == NULL || out == NULL)
  {
    return false;
  }
  if (strncmp(p, "mean", 4) == 0)
  {
    p += 4;
    if (!parse_u8(p, &p, &rule.n) || *p != '\0')
    {
      return false;
    }
    rule.mode = BOOMDETECT_ALARM_MEAN;
  }
  else
  {
    if (!parse_u8(p, &p, &rule.k_on) || strncmp(p, "of", 2) != 0)
    {
      return false;
    }
    p += 2;
    if (!parse_u8(p, &p, &rule.n) || *p != '\0')
    {
      return false;
    }
    /* Release one below the onset, at least 1: the hysteresis the training
       package's parse_rule() gives the same token. */
    rule.k_off = (rule.k_on > 1u) ? (uint8_t)(rule.k_on - 1u) : 1u;
    rule.mode  = BOOMDETECT_ALARM_VOTE;
  }
  boomdetect_alarm_t probe;
  if (!boomdetect_alarm_init(&probe, &rule))
  {
    return false;
  }
  *out = rule;
  return true;
}

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

/* Cycle-accurate timing of the hot operations (250 MHz -> 250 cyc/us).
   Cheap enough to keep always-on; reported on the debug breadcrumbs. */
static uint32_t s_half_us; /* last mic_poll, including the PDM conversion */
static uint32_t s_step_us; /* last boomdetect_step, i.e. one frame of DSP */
/* Worst cases over a run - PDM block, frame, network slice, whole loop pass -
   for DETDBG. */
static uint32_t s_half_max, s_step_max, s_work_max, s_loop_max;

static void det_max(uint32_t *max, uint32_t us)
{
  if (us > *max)
  {
    *max = us;
  }
}

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
  det_print("DETEND windows=0 drones=0 alarms=0 first_drone=- first_alarm=- overrun=0 err=1\r\n");
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
      det_max(&s_half_max, s_half_us);
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
    det_max(&s_step_max, s_step_us);
  }
  return got;
}

/* One slice of a network's decision, timed. Returns whether it finished. */
static bool det_work_timed(boomdetect_event_t *ev)
{
  const uint32_t c0   = DWT->CYCCNT;
  const bool     done = boomdetect_work(&s_det, ev);
  det_max(&s_work_max, (DWT->CYCCNT - c0) / 250u);
  return done;
}

/* Microdegrees as "-d.dddddd". */
static void fmt_e6(char *dst, size_t len, int32_t v)
{
  uint32_t mag = (v < 0) ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
  snprintf(dst, len, "%s%lu.%06lu", (v < 0) ? "-" : "", (unsigned long)(mag / 1000000u),
           (unsigned long)(mag % 1000000u));
}

/* ALARM after an ALM ON: where the unit was, from the last GPS fix. age is how
   old that fix is (s), utc when the module took it; all "-" without a fix. */
static void det_report_alarm(uint32_t t_ms)
{
  char      line[160];
  char      lat[16], lon[16], sats[8], hdop[12], age[16], utc[32];
  gps_fix_t fix;

  if (!gps_last_fix(&fix))
  {
    snprintf(line, sizeof(line), "ALARM t=%lu.%03lu lat=- lon=- sats=- hdop=- age=- utc=-\r\n",
             (unsigned long)(t_ms / 1000u), (unsigned long)(t_ms % 1000u));
    det_print(line);
    return;
  }
  const uint32_t age_ds = (HAL_GetTick() - fix.tick_ms) / 100u;
  const uint32_t hms    = fix.utc_hms;
  fmt_e6(lat, sizeof(lat), fix.lat_e6);
  fmt_e6(lon, sizeof(lon), fix.lon_e6);
  if (fix.sats == 0u)
  {
    snprintf(sats, sizeof(sats), "-");
    snprintf(hdop, sizeof(hdop), "-");
  }
  else
  {
    snprintf(sats, sizeof(sats), "%u", (unsigned)fix.sats);
    snprintf(hdop, sizeof(hdop), "%u.%u", (unsigned)(fix.hdop_x10 / 10u),
             (unsigned)(fix.hdop_x10 % 10u));
  }
  snprintf(age, sizeof(age), "%lu.%lu", (unsigned long)(age_ds / 10u),
           (unsigned long)(age_ds % 10u));
  if (fix.date_dmy != 0u)
  {
    snprintf(utc, sizeof(utc), "20%02lu-%02lu-%02luT%02lu:%02lu:%02luZ",
             (unsigned long)(fix.date_dmy % 100u), (unsigned long)(fix.date_dmy / 100u % 100u),
             (unsigned long)(fix.date_dmy / 10000u), (unsigned long)(hms / 10000u),
             (unsigned long)(hms / 100u % 100u), (unsigned long)(hms % 100u));
  }
  else
  {
    snprintf(utc, sizeof(utc), "%02lu:%02lu:%02luZ", (unsigned long)(hms / 10000u),
             (unsigned long)(hms / 100u % 100u), (unsigned long)(hms % 100u));
  }
  snprintf(line, sizeof(line), "ALARM t=%lu.%03lu lat=%s lon=%s sats=%s hdop=%s age=%s utc=%s\r\n",
           (unsigned long)(t_ms / 1000u), (unsigned long)(t_ms % 1000u), lat, lon, sats, hdop,
           age, utc);
  det_print(line);
}

static void det_report_window(const boomdetect_event_t *ev);

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

  det_report_window(ev);
}

/* DET and, on a change, ALM for a window that got its decision - in the step
   that closed it, or for a network a few blocks later from boomdetect_work. */
static void det_report_window(const boomdetect_event_t *ev)
{
  char line[80];
  char dec_str[16];

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
    if (ev->window.is_drone && s_first_drone_ms == UINT32_MAX)
    {
      s_first_drone_ms = t_ms;
    }

    /* The alarm is reported only when it changes, so a run over a steady drone
       prints one ALM ON and one ALM OFF, not one line per window. The vote
       rule reports its hits, the mean rule its mean, each over the last n. */
    if (boomdetect_alarm_push_decision(&s_alarm, ev->window.decision - s_thr))
    {
      const char *state = boomdetect_alarm_on(&s_alarm) ? "ON" : "OFF";
      if (boomdetect_alarm_on(&s_alarm) && s_first_alarm_ms == UINT32_MAX)
      {
        s_first_alarm_ms = t_ms;
      }
      if (s_alarm.rule.mode == BOOMDETECT_ALARM_MEAN)
      {
        fmt_milli(dec_str, sizeof(dec_str), boomdetect_alarm_mean(&s_alarm));
        snprintf(line, sizeof(line), "ALM t=%lu.%03lu %s mean=%s/%u\r\n",
                 (unsigned long)(t_ms / 1000u), (unsigned long)(t_ms % 1000u), state, dec_str,
                 (unsigned)s_alarm.rule.n);
      }
      else
      {
        snprintf(line, sizeof(line), "ALM t=%lu.%03lu %s hits=%u/%u\r\n",
                 (unsigned long)(t_ms / 1000u), (unsigned long)(t_ms % 1000u), state,
                 (unsigned)boomdetect_alarm_hits(&s_alarm), (unsigned)s_alarm.rule.n);
      }
      det_print(line);
      if (boomdetect_alarm_on(&s_alarm))
      {
        det_report_alarm(t_ms);
      }
    }
  }
}

void detect_service_run(uint32_t seconds, uint32_t squelch_milli, int32_t thr_milli,
                        uint32_t debug, const boomdetect_alarm_rule_t *rule)
{
  static uint8_t s_cyccnt_ready = 0u;
  char           line[128]; /* the DETEND trailer with its two times is ~100 chars */
  char           first_drone[16];
  char           first_alarm[16];

  if (!usb_cli_connected())
  {
    return;
  }
  /* 0 means "until a key is pressed"; anything else is clamped to the header's
     limit, which cli.c already enforces. Every path still ends in DETEND: a
     host driving this directly must never wait for a trailer that cannot come. */
  const bool until_key = (seconds == 0u);
  if (seconds > DETECT_MAX_SECONDS)
  {
    seconds = DETECT_MAX_SECONDS;
  }

  if (!s_cyccnt_ready)
  {
    cyccnt_init();
    s_cyccnt_ready = 1u;
  }

  /* The extractor for the model's layout, which boomdetect_init() would also
     pick by itself; looked up here so that a missing one gets its own DETERR. */
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
    .job_budget    = DETECT_JOB_BUDGET, /* only a model with a job (a network) uses it */
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
  s_thr            = (float)thr_milli / 1000.0f;
  s_first_drone_ms = UINT32_MAX;
  s_first_alarm_ms = UINT32_MAX;
  s_half_max = s_step_max = s_work_max = s_loop_max = 0u;
  if (!boomdetect_alarm_init(&s_alarm, (rule != NULL) ? rule : &s_alarm_default))
  {
    /* cli.c validates a given rule, so only an inconsistent DETECT_ALARM_*
       default gets here - say so rather than run an alarm that never fires. */
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
  /* Keeps the fix fresh for ALARM lines. A UART failure only costs the
     position (the last fix, or "-"), not the run. */
  (void)gps_track_start();

  bool mic_got = false;
  bool mic_ok  = true;

  /* 64-bit so that an open-ended run never reaches its bound. */
  const uint64_t halves =
      until_key ? UINT64_MAX
                : ((uint64_t)seconds * PCM_FS_HZ + PCM_SAMPLES_PER_HALF - 1u) /
                      PCM_SAMPLES_PER_HALF;

  for (uint64_t h = 0u; h < halves; h++)
  {
    /* Polled once per mic block (~21 ms), before waiting for it, so the run
       stops within a block of the keystroke. Only the open-ended run listens -
       a timed run is what a script drives, and a script's next command must not
       cut its own measurement short. */
    if (until_key && usb_cli_key_pressed())
    {
      break;
    }
    size_t nsamp = 0u;
    if (!det_wait_block(&nsamp))
    {
      mic_ok = false;
      break;
    }
    mic_got = true;
    const uint32_t loop_c0 = DWT->CYCCNT;

    boomdetect_push(&s_det, s_pcm, nsamp);

    /* At most one frame per mic block, which is why this is an `if` and not a
       `while`: two MFCCs in one iteration overran the 21.33 ms budget and
       starved the USB stack. Average inflow is 341 samples per block against
       512 consumed per frame, so the FIFO cannot grow unbounded regardless. */
    usb_cli_pump();
    boomdetect_event_t ev;
    bool               closed = false;
    if (det_step_timed(&ev))
    {
      det_report(&ev, debug);
      closed = ev.window.complete || ev.window.pending || ev.window.warming;
    }
    /* A network's decision runs a slice per block - but not in the block whose
       frame just closed a window: that one already did the window's feature
       extraction, the heaviest frame there is. */
    if (!closed && boomdetect_job_pending(&s_det))
    {
      boomdetect_event_t wev;
      if (det_work_timed(&wev))
      {
        det_report_window(&wev);
      }
    }
    gps_track_poll(); /* ~20 bytes per block at 9600 Bd */
    det_max(&s_loop_max, s_half_us + (DWT->CYCCNT - loop_c0) / 250u);
  }

  mic_stop();
  gps_track_poll();
  gps_track_stop();

  /* The last window's network may still be running: finish it, there is no
     block to keep up with any more. */
  while (boomdetect_job_pending(&s_det))
  {
    boomdetect_event_t wev;
    if (boomdetect_work(&s_det, &wev))
    {
      det_report_window(&wev);
    }
  }
  if (debug)
  {
    snprintf(line, sizeof(line),
             "DETDBG h_max=%lu m_max=%lu w_max=%lu loop_max=%lu skipped=%lu\r\n",
             (unsigned long)s_half_max, (unsigned long)s_step_max, (unsigned long)s_work_max,
             (unsigned long)s_loop_max, (unsigned long)boomdetect_jobs_skipped(&s_det));
    det_print(line);
  }

  uint32_t windows = 0u, drones = 0u;
  boomdetect_counts(&s_det, &windows, &drones);
  fmt_when(first_drone, sizeof(first_drone), s_first_drone_ms);
  fmt_when(first_alarm, sizeof(first_alarm), s_first_alarm_ms);
  snprintf(line, sizeof(line),
           "DETEND windows=%lu drones=%lu alarms=%lu first_drone=%s first_alarm=%s "
           "overrun=%u err=%u\r\n",
           (unsigned long)windows, (unsigned long)drones, (unsigned long)s_alarm.onsets,
           first_drone, first_alarm,
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
