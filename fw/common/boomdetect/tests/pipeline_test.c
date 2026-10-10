/**
 * @file pipeline_test.c
 * @brief Edge cases of the decimation, FIFO, framing and windowing.
 *
 * These are the parts that moved out of the firmware, and the parts the model
 * was NOT trained against - training slid windows with a hop of 7 over
 * contiguous audio, the firmware takes disjoint runs of 14 and gates each frame
 * on its own level. So this file pins the firmware's behaviour rather than
 * claiming it is the right one; whether the two agree is the parity suite's
 * question, not this one's.
 */
#include "bd_test.h"
#include "boomdetect.h"

#include <math.h>
#include <string.h>

BD_TEST_STATE;

/* A stub, so nothing here depends on the real weights: returns feature[0] so a
   decision is predictable from the input. */
static float stub_decide(void *ctx, const float *features, uint16_t n)
{
    (void)n;
    (void)ctx;
    return features[0];
}

static const classifier_t stub_model = {
    .name = "stub",
    .layout_id = BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX,
    .n_features = 8u,
    .feature_offset = 0u,
    .default_thr_milli = 0,
    .decide = stub_decide,
    .ctx = NULL,
};

static boomdetect_config_t base_cfg(uint32_t squelch_milli)
{
    boomdetect_config_t cfg = {
        .decimation = 3u,
        .squelch_milli = squelch_milli,
        .thr_milli = 0,
        .classifier = &stub_model,
    };
    return cfg;
}

/* Deterministic and loud enough to clear any squelch these tests set. */
static int16_t tone(uint32_t i)
{
    static const int16_t lut[8] = { 0, 6000, 8000, 6000, 0, -6000, -8000, -6000 };
    return lut[i % 8u];
}

static void scenario_nothing_from_nothing(void)
{
    static boomdetect_t d;
    boomdetect_config_t cfg = base_cfg(0u);
    boomdetect_event_t ev;
    REQUIRE(boomdetect_init(&d, &cfg), "init failed");

    CHECK(!boomdetect_step(&d, &ev), "an empty detector produced a frame");

    /* Fewer input samples than the decimation factor: nothing to keep, and in
       particular no divide-by-zero or wrap in the phase carry. */
    const int16_t two[2] = { 1, 2 };
    boomdetect_push(&d, two, 2u);
    CHECK(!boomdetect_step(&d, &ev), "two input samples produced a frame");
    CHECK(boomdetect_dropped(&d) == 0u, "two samples were counted as dropped");
}

/* One frame needs BOOMDETECT_WINDOW_SIZE decimated samples; the boundary is worth pinning
   because an off-by-one here shifts every frame in the run. */
static void scenario_frame_boundary(void)
{
    static boomdetect_t d;
    boomdetect_config_t cfg = base_cfg(0u);
    boomdetect_event_t ev;
    REQUIRE(boomdetect_init(&d, &cfg), "init failed");

    static int16_t buf[BOOMDETECT_WINDOW_SIZE * 3u];
    for (uint32_t i = 0u; i < BOOMDETECT_WINDOW_SIZE * 3u; i++)
    {
        buf[i] = tone(i);
    }

    /* One sample short of a full frame. */
    boomdetect_push(&d, buf, (BOOMDETECT_WINDOW_SIZE - 1u) * 3u);
    CHECK(!boomdetect_step(&d, &ev), "a frame appeared %u samples early", 1u);

    boomdetect_push(&d, buf, 3u); /* the sample that completes it */
    CHECK(boomdetect_step(&d, &ev), "a full frame did not produce an event");
    CHECK(ev.frame_index == 0u, "first frame reported index %u", ev.frame_index);
    CHECK(!ev.window.complete, "one frame completed a whole window");
}

/* Block size must not change the result: the phase carry across pushes is what
   makes that true, and it is easy to get subtly wrong. */
static void scenario_block_size_is_irrelevant(void)
{
    static boomdetect_t a, b;
    boomdetect_config_t cfg = base_cfg(0u);
    boomdetect_event_t ev;
    static int16_t buf[BOOMDETECT_WINDOW_SIZE * 3u * 2u];
    /* The input is BOOMDETECT_WINDOW_SIZE*6 samples at 48 kHz, so BOOMDETECT_WINDOW_SIZE*2 after /3,
       which is one whole frame plus (BOOMDETECT_WINDOW_SIZE/HOP - 1) hops: 3 frames.
       Sized to that and asserted below, because an array of 8 with a `< 8u` cap
       reads as if eight frames were being compared when the cap never engages. */
    enum { EXPECTED_FRAMES = 3u };
    float dec_a[EXPECTED_FRAMES], dec_b[EXPECTED_FRAMES];
    uint32_t na = 0u, nb = 0u;

    for (uint32_t i = 0u; i < sizeof(buf) / sizeof(buf[0]); i++)
    {
        buf[i] = tone(i);
    }

    REQUIRE(boomdetect_init(&a, &cfg), "init a failed");
    boomdetect_push(&a, buf, sizeof(buf) / sizeof(buf[0]));
    while (na < EXPECTED_FRAMES && boomdetect_step(&a, &ev))
    {
        dec_a[na++] = ev.rms;
    }

    /* Same signal, one sample at a time, and stepping as soon as anything is
       ready - so the FIFO is at a different fill level at every frame. */
    REQUIRE(boomdetect_init(&b, &cfg), "init b failed");
    for (uint32_t i = 0u; i < sizeof(buf) / sizeof(buf[0]); i++)
    {
        boomdetect_push(&b, &buf[i], 1u);
        while (nb < EXPECTED_FRAMES && boomdetect_step(&b, &ev))
        {
            dec_b[nb++] = ev.rms;
        }
    }

    REQUIRE(na == nb, "block feeding gave %u frames, sample feeding %u", na, nb);
    CHECK(na == EXPECTED_FRAMES,
          "this input should yield exactly %u frames, got %u - if that changed, the "
          "cap above is silently truncating the comparison",
          (unsigned)EXPECTED_FRAMES, na);
    for (uint32_t i = 0u; i < na; i++)
    {
        CHECK(dec_a[i] == dec_b[i],
              "frame %u differs by block size: %.9g vs %.9g - the decimation phase "
              "is not being carried across pushes", i, (double)dec_a[i], (double)dec_b[i]);
    }
}

/* The FIFO must drop rather than overwrite: overwriting unread samples corrupts
   a window's features silently, which is far worse than a reported gap. */
static void scenario_overflow_drops_and_reports(void)
{
    static boomdetect_t d;
    boomdetect_config_t cfg = base_cfg(0u);
    REQUIRE(boomdetect_init(&d, &cfg), "init failed");

    static int16_t buf[BOOMDETECT_RING_LEN * 3u * 2u];
    for (uint32_t i = 0u; i < sizeof(buf) / sizeof(buf[0]); i++)
    {
        buf[i] = tone(i);
    }
    /* Twice the FIFO, with nothing consuming it. */
    boomdetect_push(&d, buf, sizeof(buf) / sizeof(buf[0]));

    CHECK(boomdetect_dropped(&d) > 0u,
          "pushing twice the FIFO's capacity dropped nothing, so it must have "
          "overwritten unread samples");
    CHECK(boomdetect_dropped(&d) == BOOMDETECT_RING_LEN,
          "expected exactly %u drops, got %u", (unsigned)BOOMDETECT_RING_LEN,
          boomdetect_dropped(&d));

    /* And it must still be usable afterwards, not wedged. */
    boomdetect_event_t ev;
    CHECK(boomdetect_step(&d, &ev), "the detector stopped producing frames after an overflow");
}

/* A window is fourteen ACCEPTED frames. A quiet frame in the middle throws the
   partial window away - that is the firmware's gating policy, and it is exactly
   where it departs from how the training windows were chosen. */
static void scenario_squelch_resets_partial_window(void)
{
    static boomdetect_t d;
    /* Gate at half the tone's level, so silence is below and the tone above. */
    boomdetect_config_t cfg = base_cfg(100u);
    boomdetect_event_t ev;
    REQUIRE(boomdetect_init(&d, &cfg), "init failed");

    static int16_t loud[BOOMDETECT_WINDOW_SIZE * 3u];
    static int16_t quiet[BOOMDETECT_WINDOW_SIZE * 3u];
    for (uint32_t i = 0u; i < BOOMDETECT_WINDOW_SIZE * 3u; i++)
    {
        loud[i] = tone(i);
        quiet[i] = 0;
    }

    /* Two frames short, not one. Frames are BOOMDETECT_WINDOW_SIZE long but advance by
       BOOMDETECT_HOP, so the first frame after the input goes quiet still
       overlaps half a frame of loud audio and passes the gate - only the second
       is genuinely silent. Stopping one short would let that mixed frame
       complete the window, which is real behaviour but not what this scenario
       is about.

       Feed until the count is reached rather than computing how many blocks it
       takes: the first frame costs two hops and every later one costs a single
       hop, an off-by-one a test should not be restating. */
    uint32_t accepted = 0u;
    uint32_t guard = 0u;
    while (accepted < BOOMDETECT_ACCUM_FRAMES - 2u && guard++ < 100u)
    {
        boomdetect_push(&d, loud, BOOMDETECT_HOP * 3u);
        /* Drain to empty, not just until the count is reached: leaving loud
           samples queued would mean the "silence" pushed below is processed
           after them, and the window would complete on the leftovers rather
           than on what the scenario is actually about. */
        while (boomdetect_step(&d, &ev))
        {
            CHECK(!ev.squelched, "a loud frame was gated out (rms %.6g)", (double)ev.rms);
            CHECK(!ev.window.complete, "a window completed before it was full");
            accepted = ev.accum;
        }
    }
    REQUIRE(accepted == BOOMDETECT_ACCUM_FRAMES - 2u,
            "expected %u accumulated frames, got %u after %u blocks",
            BOOMDETECT_ACCUM_FRAMES - 2u, accepted, guard);

    bool saw_mixed = false;
    bool saw_squelch = false;
    for (uint32_t f = 0u; f < 3u; f++)
    {
        boomdetect_push(&d, quiet, BOOMDETECT_HOP * 3u);
        while (boomdetect_step(&d, &ev))
        {
            if (!ev.squelched && !saw_squelch)
            {
                /* The overlap frame: half loud, so still above the gate. */
                saw_mixed = true;
                CHECK(ev.accum == BOOMDETECT_ACCUM_FRAMES - 1u,
                      "the half-loud overlap frame left %u accumulated, expected %u",
                      ev.accum, BOOMDETECT_ACCUM_FRAMES - 1u);
            }
            if (ev.squelched)
            {
                saw_squelch = true;
                CHECK(ev.accum == 0u,
                      "a gated frame left %u frames accumulated instead of resetting",
                      ev.accum);
            }
            CHECK(!ev.window.complete,
                  "a window completed although silence should have reset the "
                  "accumulation first");
        }
    }
    CHECK(saw_mixed, "no half-loud overlap frame was seen; the framing may not overlap");
    CHECK(saw_squelch, "silence was not gated at squelch_milli=100");

    uint32_t windows = 0u, drones = 0u;
    boomdetect_counts(&d, &windows, &drones);
    CHECK(windows == 0u, "%u windows were classified when none should have been", windows);
}

/* Fourteen accepted frames in a row must complete exactly one window, and the
   decision must come from the slice the model declares. */
static void scenario_window_completes_and_dispatches(void)
{
    static boomdetect_t d;
    boomdetect_config_t cfg = base_cfg(0u);
    boomdetect_event_t ev;
    REQUIRE(boomdetect_init(&d, &cfg), "init failed");

    static int16_t loud[BOOMDETECT_WINDOW_SIZE * 3u];
    for (uint32_t i = 0u; i < BOOMDETECT_WINDOW_SIZE * 3u; i++)
    {
        loud[i] = tone(i);
    }

    uint32_t completed = 0u;
    float decision = 0.0f;
    for (uint32_t f = 0u; f < BOOMDETECT_ACCUM_FRAMES + 2u; f++)
    {
        boomdetect_push(&d, loud, BOOMDETECT_HOP * 3u);
        while (boomdetect_step(&d, &ev))
        {
            if (ev.window.complete)
            {
                completed++;
                decision = ev.window.decision;
                CHECK(ev.window.start_frame == 0u,
                      "first window started at frame %u", ev.window.start_frame);
                CHECK(ev.accum == 0u,
                      "the accumulator was not reset after a window (%u left)", ev.accum);
            }
        }
    }
    REQUIRE(completed == 1u, "expected exactly one window, got %u", completed);

    /* stub_decide returns features[0], and stub_model reads from offset 0, so
       the decision must equal the first aggregated feature. That is what proves
       the offset reached decide() rather than being ignored. */
    const float *feat = boomdetect_last_features(&d);
    REQUIRE(feat != NULL, "no feature vector after a completed window");
    CHECK(decision == feat[0],
          "decision %.9g is not features[offset] %.9g - the slice handed to "
          "decide() is not the one the model declared",
          (double)decision, (double)feat[0]);

    uint32_t windows = 0u, drones = 0u;
    boomdetect_counts(&d, &windows, &drones);
    CHECK(windows == 1u, "counted %u windows", windows);
    CHECK(drones == (decision >= 0.0f ? 1u : 0u),
          "drone count %u does not match a threshold of 0 against decision %.9g",
          drones, (double)decision);
}

/* Same input, non-zero offset: the decision must follow the offset. */
static void scenario_offset_is_honoured(void)
{
    static classifier_t shifted = {
        .name = "stub_shifted",
        .layout_id = BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX,
        .n_features = 8u,
        .feature_offset = BOOMDETECT_MFCC_COEFFS, /* first std, rather than first mean */
        .default_thr_milli = 0,
        .decide = stub_decide,
        .ctx = NULL,
    };
    static boomdetect_t d;
    boomdetect_config_t cfg = base_cfg(0u);
    boomdetect_event_t ev;
    cfg.classifier = &shifted;
    REQUIRE(boomdetect_init(&d, &cfg), "init failed");

    static int16_t loud[BOOMDETECT_WINDOW_SIZE * 3u];
    for (uint32_t i = 0u; i < BOOMDETECT_WINDOW_SIZE * 3u; i++)
    {
        loud[i] = tone(i);
    }

    float decision = 0.0f;
    bool got = false;
    for (uint32_t f = 0u; f < BOOMDETECT_ACCUM_FRAMES + 2u && !got; f++)
    {
        boomdetect_push(&d, loud, BOOMDETECT_HOP * 3u);
        while (boomdetect_step(&d, &ev))
        {
            if (ev.window.complete)
            {
                decision = ev.window.decision;
                got = true;
            }
        }
    }
    REQUIRE(got, "no window completed");

    const float *feat = boomdetect_last_features(&d);
    REQUIRE(feat != NULL, "no feature vector after a completed window");
    CHECK(decision == feat[BOOMDETECT_MFCC_COEFFS],
          "with offset %u the decision should be features[%u] (%.9g), got %.9g",
          (unsigned)BOOMDETECT_MFCC_COEFFS, (unsigned)BOOMDETECT_MFCC_COEFFS,
          (double)feat[BOOMDETECT_MFCC_COEFFS], (double)decision);
    CHECK(decision != feat[0],
          "the decision equals features[0] even though the model declared offset %u, "
          "so the offset is being ignored", (unsigned)BOOMDETECT_MFCC_COEFFS);
}

/* A model with a job, five steps long whatever the budget below 5000: decide()
   and the job agree on features[0], so a deferred decision is predictable. */
#define JOB_STEPS 5u
static float    s_job_value;
static uint32_t s_job_left;
static uint32_t s_job_begins;

static bool job_begin(void *ctx, const float *features, uint16_t n)
{
    (void)ctx;
    (void)n;
    s_job_value = features[0];
    s_job_left  = JOB_STEPS;
    s_job_begins++;
    return true;
}

static bool job_run(void *ctx, uint32_t budget)
{
    (void)ctx;
    if (s_job_left > 0u)
    {
        s_job_left = (budget >= 5000u) ? 0u : s_job_left - 1u;
    }
    return s_job_left == 0u;
}

static float job_result(void *ctx)
{
    (void)ctx;
    return s_job_value;
}

static const classifier_job_t s_stub_job = {
    .begin = job_begin, .run = job_run, .result = job_result,
};

static const classifier_t job_model = {
    .name = "stub_job",
    .layout_id = BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX,
    .n_features = 8u,
    .decide = stub_decide,
    .job = &s_stub_job,
};

/* job_budget > 0: the window closes pending, its decision comes out of
   boomdetect_work() a few calls later with the window's own frames, and a window
   closing meanwhile is dropped and counted. job_budget 0: the same model decides
   inline like any other. */
static void scenario_deferred_decision(void)
{
    static boomdetect_t d;
    static int16_t      loud[BOOMDETECT_WINDOW_SIZE * 3u];
    for (uint32_t i = 0u; i < BOOMDETECT_WINDOW_SIZE * 3u; i++)
    {
        loud[i] = tone(i);
    }
    boomdetect_config_t cfg = base_cfg(0u);
    boomdetect_event_t  ev, wev;
    cfg.classifier = &job_model;
    cfg.job_budget = 1000u;
    REQUIRE(boomdetect_init(&d, &cfg), "init failed");
    CHECK(!boomdetect_work(&d, &wev), "work() did something with no window closed");

    uint32_t pending = 0u, start = 0u, end = 0u;
    for (uint32_t f = 0u; f < BOOMDETECT_ACCUM_FRAMES + 2u && pending == 0u; f++)
    {
        boomdetect_push(&d, loud, BOOMDETECT_HOP * 3u);
        while (boomdetect_step(&d, &ev))
        {
            CHECK(!ev.window.complete, "a job model completed a window inside step()");
            if (ev.window.pending)
            {
                pending++;
                start = ev.window.start_frame;
                end   = ev.window.end_frame;
                CHECK(isnan(ev.window.decision), "a pending window carried decision %.9g",
                      (double)ev.window.decision);
                break;
            }
        }
    }
    REQUIRE(pending == 1u, "no window closed pending");
    CHECK(boomdetect_job_pending(&d), "the detector does not report its job");
    uint32_t windows = 99u;
    boomdetect_counts(&d, &windows, NULL);
    CHECK(windows == 0u, "a pending window was counted (%u)", windows);

    uint32_t calls = 0u;
    bool     done  = false;
    while (!done && calls < 20u)
    {
        done = boomdetect_work(&d, &wev);
        calls++;
    }
    REQUIRE(done, "the job never finished");
    CHECK(calls == JOB_STEPS, "finished after %u work() calls, the job has %u steps", calls,
          JOB_STEPS);
    const float *feat = boomdetect_last_features(&d);
    REQUIRE(feat != NULL, "no feature vector after the window");
    CHECK(wev.window.complete && wev.window.decision == feat[0],
          "the deferred decision %.9g is not features[0] %.9g", (double)wev.window.decision,
          (double)feat[0]);
    CHECK(wev.window.start_frame == start && wev.window.end_frame == end,
          "the decision came for frames %u..%u, the window was %u..%u", wev.window.start_frame,
          wev.window.end_frame, start, end);
    CHECK(wev.window.is_drone == (wev.window.decision >= 0.0f), "is_drone does not follow thr 0");
    boomdetect_counts(&d, &windows, NULL);
    CHECK(windows == 1u && !boomdetect_job_pending(&d), "after the decision: %u windows, pending %d",
          windows, (int)boomdetect_job_pending(&d));
    CHECK(!boomdetect_work(&d, &wev), "work() returned a second decision for one window");

    /* Two more windows without any work in between: the second closes while the
       first one's job is still running, and is dropped. */
    const uint32_t begins = s_job_begins;
    uint32_t       closed = 0u;
    for (uint32_t f = 0u; f < 2u * BOOMDETECT_ACCUM_FRAMES + 2u; f++)
    {
        boomdetect_push(&d, loud, BOOMDETECT_HOP * 3u);
        while (boomdetect_step(&d, &ev))
        {
            closed += ev.window.pending ? 1u : 0u;
        }
    }
    CHECK(closed == 1u && s_job_begins == begins + 1u,
          "%u windows closed pending, %u jobs begun; expected one of each", closed,
          s_job_begins - begins);
    CHECK(boomdetect_jobs_skipped(&d) >= 1u, "the dropped window was not counted");

    /* No budget: the job model is just a model. */
    cfg.job_budget = 0u;
    REQUIRE(boomdetect_init(&d, &cfg), "init failed");
    bool inline_done = false;
    for (uint32_t f = 0u; f < BOOMDETECT_ACCUM_FRAMES + 2u && !inline_done; f++)
    {
        boomdetect_push(&d, loud, BOOMDETECT_HOP * 3u);
        while (boomdetect_step(&d, &ev))
        {
            CHECK(!ev.window.pending, "job_budget 0 still deferred a window");
            inline_done = inline_done || ev.window.complete;
        }
    }
    CHECK(inline_done && !boomdetect_job_pending(&d), "job_budget 0 did not decide inline");
}

int main(void)
{
    scenario_nothing_from_nothing();
    scenario_frame_boundary();
    scenario_block_size_is_irrelevant();
    scenario_overflow_drops_and_reports();
    scenario_squelch_resets_partial_window();
    scenario_window_completes_and_dispatches();
    scenario_offset_is_honoured();
    scenario_deferred_decision();
    BD_TEST_REPORT("pipeline_test", 114);  /* exact count from running the compiled binary */
}
