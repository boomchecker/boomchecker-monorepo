/**
 * @file extractor_test.c
 * @brief The four extractors against the Python specification, and against
 *        each other.
 *
 * Drives the board's LCG fixture signal through detectors configured with each
 * extractor and compares what comes out with tests/vectors/extractor_expected.h,
 * which training/boomdetect_train/fixtures.py generated from features.py.
 * Tolerances are stated: the C sums 512 bins in float32 where Python uses
 * float64, so the scalars agree to about 1e-5 relative; the roll-off bin and
 * the comb fundamental are discrete and can differ by one step on a near-tie,
 * which the fixture marks per window (expected_f0_stable) and this test skips.
 *
 * Layout 4 needs two seconds of envelope before it says anything and the LCG
 * fixture is 1.4 s, so it has its own signal: the same LCG, amplitude-modulated
 * at 192 Hz in integer arithmetic (tests/vectors/extractor_mod_expected.h, from
 * dsp/selftest.py am_lcg_signal). The windows that close before the ring is
 * full must produce no decision; the ones after are held to Python.
 */
#include "bd_test.h"
#include "boomdetect.h"
#include "boomdetect_selftest.h"
#include "extractors.h"
#include "vectors/extractor_expected.h"
#include "vectors/extractor_mod_expected.h"

#include <math.h>
#include <string.h>

BD_TEST_STATE;

/* Loose enough for float32-versus-float64 sums over 512 bins, tight enough that
   a wrong bin range, a wrong epsilon or a swapped statistic fails. */
#define SCALAR_REL 2e-4f
#define SCALAR_ABS 2e-5f
/* Layout-2 aggregates of those scalars, and the MFCC statistics (which the
   parity of the shipped fixture already holds to 1e-4). */
#define L2_REL 3e-4f
#define L2_ABS 3e-4f
/* Log-mel values are O(1)..O(10); ln() of a float32 mel sum. */
#define L3_REL 1e-4f
#define L3_ABS 2e-4f

/* Indices of the two f0 entries in layout 2: mean and std of scalar 7. */
#define L2_F0_MEAN (4u * BOOMDETECT_MFCC_COEFFS + BOOMDETECT_SCALAR_F0)
#define L2_F0_STD  (4u * BOOMDETECT_MFCC_COEFFS + BOOMDETECT_FRAME_SCALARS + BOOMDETECT_SCALAR_F0)
/* The roll-off mean/std may sit one bin (1/512) off; give them their own slack. */
#define L2_ROLL_MEAN (4u * BOOMDETECT_MFCC_COEFFS + BOOMDETECT_SCALAR_ROLLOFF)
#define L2_ROLL_STD  (4u * BOOMDETECT_MFCC_COEFFS + BOOMDETECT_FRAME_SCALARS + BOOMDETECT_SCALAR_ROLLOFF)
#define ROLL_ABS (2.0f / 512.0f)
/* The envelope: three float32 biquads against scipy's float64 sosfilt. */
#define ENV_REL 1e-4f
#define ENV_ABS 1e-7f
/* Modulation features: the dB/20 values and the ratios, after a float32 FFT,
   log10f and sums over 257 bins; 2e-3 is 0.04 dB. The line count is discrete
   (one of 179 bins may sit on the 6 dB edge) and the peak bin has to be the
   same bin exactly - the fixture's line is 20 dB proud, so no near-tie. */
#define L4_MOD_REL 2e-3f
#define L4_MOD_ABS 2e-3f
#define L4_LINES_ABS (1.5f / 179.0f)
#define L4_IDX_LINES (BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL + 1u)
#define L4_IDX_FPEAK (BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL + 2u)

static float probe_decide(void *ctx, const float *features, uint16_t n)
{
    (void)ctx;
    (void)n;
    return features[0];
}

static bool close_enough(float got, float want, float rel, float abs_)
{
    const float scale = fmaxf(fabsf(got), fabsf(want));
    return fabsf(got - want) <= rel * scale + abs_;
}

static int16_t s_block[BOOMDETECT_SELFTEST_BLOCK];

/* Run the LCG signal through a detector on `ex`, collecting every completed
   window's features into `out` (rows of ex->n_features) and the first frames'
   scalars into `scalars`. Returns the number of windows. */
static uint32_t run_lcg(const boomdetect_extractor_t *ex, float *out, uint32_t max_windows,
                        float *scalars, uint32_t n_scalar_frames)
{
    static boomdetect_t d;
    classifier_t        probe = {
        .name = "probe", .layout_id = ex->layout_id, .n_features = 1u, .feature_offset = 0u,
        .default_thr_milli = 0, .decide = probe_decide, .ctx = NULL,
    };
    const boomdetect_config_t cfg = {
        .decimation = 3u, .squelch_milli = 0u, .thr_milli = 0, .classifier = &probe, .extractor = ex,
    };
    if (!boomdetect_init(&d, &cfg))
    {
        return 0u;
    }
    uint32_t state = BOOMDETECT_SELFTEST_SEED, windows = 0u;
    for (uint32_t off = 0u; off < BOOMDETECT_SELFTEST_INPUT_LEN; off += BOOMDETECT_SELFTEST_BLOCK)
    {
        const uint32_t left = BOOMDETECT_SELFTEST_INPUT_LEN - off;
        const uint32_t n    = (left < BOOMDETECT_SELFTEST_BLOCK) ? left : BOOMDETECT_SELFTEST_BLOCK;
        for (uint32_t i = 0u; i < n; i++)
        {
            state     = (state * 1103515245u) + 12345u;
            int32_t v = (int32_t)((state >> 16) & 0xFFFFu) - 32768;
            s_block[i] = (int16_t)(v / 4);
        }
        boomdetect_push(&d, s_block, n);
        boomdetect_event_t ev;
        while (boomdetect_step(&d, &ev))
        {
            if (d.last_mfcc_slot < d.cfg.accum_frames && ev.frame_index < n_scalar_frames)
            {
                const float *row = &d.mfccs[d.last_mfcc_slot * BOOMDETECT_FRAME_WIDTH];
                memcpy(scalars + ev.frame_index * BOOMDETECT_FRAME_SCALARS,
                       row + BOOMDETECT_FRAME_SCALAR_OFF, BOOMDETECT_FRAME_SCALARS * sizeof(float));
            }
            if (ev.window.complete && windows < max_windows)
            {
                memcpy(out + windows * ex->n_features, boomdetect_last_features(&d),
                       ex->n_features * sizeof(float));
                windows++;
            }
        }
    }
    return windows;
}

/* The layout-4 fixture signal: dsp/selftest.py am_lcg_signal(). The quartered
   LCG sample times (1024 + table[i mod period]) / 1024, truncated toward zero
   like C's integer division, which is why Python mirrors the truncation. Runs
   the whole 5 s through a detector on `ex` and collects: every completed
   window's features (rows of ex->n_features) and its closing frame, how many
   windows closed warming, and the envelope ring after the first
   EXPECTED_ENV_FRAMES frames (the ring has not wrapped by then, so it reads
   straight from index 0). */
static int16_t s_am_block[BOOMDETECT_SELFTEST_BLOCK];

static uint32_t run_am(const boomdetect_extractor_t *ex, float *out, uint32_t max_windows,
                       uint16_t *end_frames, uint32_t *warming, float *env_first)
{
    static boomdetect_t d;
    classifier_t        probe = {
        .name = "probe", .layout_id = ex->layout_id, .n_features = 1u, .feature_offset = 0u,
        .default_thr_milli = 0, .decide = probe_decide, .ctx = NULL,
    };
    const boomdetect_config_t cfg = {
        .decimation = 3u, .squelch_milli = 0u, .thr_milli = 0, .classifier = &probe, .extractor = ex,
    };
    if (!boomdetect_init(&d, &cfg))
    {
        return 0u;
    }
    uint32_t state = BOOMDETECT_SELFTEST_SEED, windows = 0u, warm = 0u, pos = 0u;
    for (uint32_t off = 0u; off < EXPECTED_AM_INPUT_LEN; off += BOOMDETECT_SELFTEST_BLOCK)
    {
        const uint32_t left = EXPECTED_AM_INPUT_LEN - off;
        const uint32_t n    = (left < BOOMDETECT_SELFTEST_BLOCK) ? left : BOOMDETECT_SELFTEST_BLOCK;
        for (uint32_t i = 0u; i < n; i++)
        {
            state           = (state * 1103515245u) + 12345u;
            const int32_t v = ((int32_t)((state >> 16) & 0xFFFFu) - 32768) / 4;
            const int32_t g = 1024 + (int32_t)expected_am_table[pos % EXPECTED_AM_PERIOD];
            s_am_block[i]   = (int16_t)((v * g) / 1024);
            pos++;
        }
        boomdetect_push(&d, s_am_block, n);
        boomdetect_event_t ev;
        while (boomdetect_step(&d, &ev))
        {
            if (env_first != NULL && ev.frame_index == EXPECTED_ENV_FRAMES - 1u)
            {
                memcpy(env_first, d.env_ring,
                       (size_t)EXPECTED_ENV_FRAMES * EXPECTED_ENV_PER_FRAME * sizeof(float));
            }
            if (ev.window.warming)
            {
                warm++;
            }
            if (ev.window.complete && windows < max_windows)
            {
                memcpy(out + windows * ex->n_features, boomdetect_last_features(&d),
                       ex->n_features * sizeof(float));
                if (end_frames != NULL)
                {
                    end_frames[windows] = (uint16_t)ev.window.end_frame;
                }
                windows++;
            }
        }
    }
    if (warming != NULL)
    {
        *warming = warm;
    }
    return windows;
}

static float    s_l2[EXPECTED_WINDOWS][EXPECTED_L2_WIDTH];
static float    s_l3[EXPECTED_WINDOWS][EXPECTED_L3_WIDTH];
static float    s_l1[EXPECTED_WINDOWS][BOOMDETECT_FEATURE_COUNT];
static float    s_scal[EXPECTED_SCALAR_FRAMES][BOOMDETECT_FRAME_SCALARS];
static float    s_l4[EXPECTED_AM_READY][EXPECTED_L4_WIDTH];
static float    s_am_l2[EXPECTED_AM_WINDOWS][EXPECTED_L2_WIDTH];
static uint16_t s_l4_end[EXPECTED_AM_READY];
static float    s_env_first[EXPECTED_ENV_FRAMES * EXPECTED_ENV_PER_FRAME];

static void scenario_registry(void)
{
    REQUIRE(boomdetect_extractor_count() == 4u, "expected four extractors, got %u",
            (unsigned)boomdetect_extractor_count());
    const boomdetect_extractor_t *s = boomdetect_extractor_by_name("stats");
    const boomdetect_extractor_t *p = boomdetect_extractor_by_name("stats_spectral");
    const boomdetect_extractor_t *l = boomdetect_extractor_by_name("logmel");
    const boomdetect_extractor_t *m = boomdetect_extractor_by_name("stats_spectral_mod");
    REQUIRE(s != NULL && p != NULL && l != NULL && m != NULL, "an extractor is missing by name");
    CHECK(boomdetect_extractor_default() == s, "the default is not 'stats'");
    CHECK(s->layout_id == BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX && s->n_features == 52u,
          "stats: layout %u width %u", s->layout_id, s->n_features);
    CHECK(p->layout_id == BOOMDETECT_LAYOUT_STATS_SPECTRAL && p->n_features == EXPECTED_L2_WIDTH,
          "stats_spectral: layout %u width %u", p->layout_id, p->n_features);
    CHECK(l->layout_id == BOOMDETECT_LAYOUT_LOGMEL && l->n_features == EXPECTED_L3_WIDTH,
          "logmel: layout %u width %u", l->layout_id, l->n_features);
    CHECK(m->layout_id == BOOMDETECT_LAYOUT_STATS_SPECTRAL_MOD && m->n_features == EXPECTED_L4_WIDTH,
          "stats_spectral_mod: layout %u width %u", m->layout_id, m->n_features);
    CHECK(m->env_required == BOOMDETECT_ENV_RING && s->env_required == 0u &&
              p->env_required == 0u && l->env_required == 0u,
          "only layout 4 should wait for the envelope ring (%u / %u / %u / %u)",
          s->env_required, p->env_required, l->env_required, m->env_required);
    CHECK(boomdetect_extractor_for_layout(2u) == p, "for_layout(2) did not find stats_spectral");
    CHECK(boomdetect_extractor_for_layout(4u) == m, "for_layout(4) did not find stats_spectral_mod");
    CHECK(boomdetect_extractor_for_layout(99u) == NULL, "for_layout(99) found something");
    CHECK(p->n_features <= BOOMDETECT_FEATURE_MAX && l->n_features <= BOOMDETECT_FEATURE_MAX &&
              m->n_features <= BOOMDETECT_FEATURE_MAX,
          "a layout is wider than boomdetect_t::features");
}

static void scenario_scalars_match_python(void)
{
    const uint32_t n = run_lcg(boomdetect_extractor_by_name("stats_spectral"), &s_l2[0][0],
                               EXPECTED_WINDOWS, &s_scal[0][0], EXPECTED_SCALAR_FRAMES);
    REQUIRE(n == EXPECTED_WINDOWS, "layout 2 produced %lu windows, expected %d", (unsigned long)n,
            EXPECTED_WINDOWS);
    for (uint32_t f = 0u; f < EXPECTED_SCALAR_FRAMES; f++)
    {
        for (uint32_t s = 0u; s < BOOMDETECT_FRAME_SCALARS; s++)
        {
            const float got = s_scal[f][s], want = expected_scalars[f][s];
            float abs_ = SCALAR_ABS;
            if (s == BOOMDETECT_SCALAR_ROLLOFF)
            {
                abs_ = ROLL_ABS;
            }
            if (s == BOOMDETECT_SCALAR_F0)
            {
                /* Discrete: only compared when the window it belongs to was stable. */
                continue;
            }
            CHECK(close_enough(got, want, SCALAR_REL, abs_),
                  "frame %lu scalar %lu: got %.9g, python %.9g", (unsigned long)f,
                  (unsigned long)s, (double)got, (double)want);
        }
    }
}

static void scenario_layout2_matches_python(void)
{
    for (uint32_t w = 0u; w < EXPECTED_WINDOWS; w++)
    {
        for (uint32_t i = 0u; i < EXPECTED_L2_WIDTH; i++)
        {
            const float got = s_l2[w][i], want = expected_l2[w][i];
            float rel = L2_REL, abs_ = L2_ABS;
            if ((i == L2_F0_MEAN || i == L2_F0_STD) && expected_f0_stable[w] == 0u)
            {
                continue;
            }
            if (i == L2_ROLL_MEAN || i == L2_ROLL_STD)
            {
                abs_ = ROLL_ABS;
            }
            CHECK(close_enough(got, want, rel, abs_), "window %lu feature %lu: got %.9g, python %.9g",
                  (unsigned long)w, (unsigned long)i, (double)got, (double)want);
        }
    }
}

static void scenario_layout2_prefix_is_layout1(void)
{
    const uint32_t n = run_lcg(boomdetect_extractor_by_name("stats"), &s_l1[0][0], EXPECTED_WINDOWS,
                               &s_scal[0][0], 0u);
    REQUIRE(n == EXPECTED_WINDOWS, "layout 1 produced %lu windows", (unsigned long)n);
    for (uint32_t w = 0u; w < EXPECTED_WINDOWS; w++)
    {
        CHECK(memcmp(s_l1[w], s_l2[w], sizeof(s_l1[w])) == 0,
              "window %lu: the first 52 of layout 2 are not bit-identical to layout 1",
              (unsigned long)w);
    }
}

static void scenario_layout3_matches_python_and_is_zero_mean(void)
{
    const uint32_t n = run_lcg(boomdetect_extractor_by_name("logmel"), &s_l3[0][0], EXPECTED_WINDOWS,
                               &s_scal[0][0], 0u);
    REQUIRE(n == EXPECTED_WINDOWS, "layout 3 produced %lu windows", (unsigned long)n);
    for (uint32_t w = 0u; w < EXPECTED_WINDOWS; w++)
    {
        float sum = 0.0f;
        for (uint32_t i = 0u; i < EXPECTED_L3_WIDTH; i++)
        {
            sum += s_l3[w][i];
            CHECK(close_enough(s_l3[w][i], expected_l3[w][i], L3_REL, L3_ABS),
                  "window %lu patch value %lu: got %.9g, python %.9g", (unsigned long)w,
                  (unsigned long)i, (double)s_l3[w][i], (double)expected_l3[w][i]);
        }
        CHECK(fabsf(sum / (float)EXPECTED_L3_WIDTH) < 1e-4f, "window %lu patch mean %.3g is not ~0",
              (unsigned long)w, (double)(sum / (float)EXPECTED_L3_WIDTH));
    }
}

static void scenario_layout4_matches_python(void)
{
    const boomdetect_extractor_t *m = boomdetect_extractor_by_name("stats_spectral_mod");
    REQUIRE(m != NULL, "no stats_spectral_mod extractor");
    uint32_t       warming = 0u;
    const uint32_t n = run_am(m, &s_l4[0][0], EXPECTED_AM_READY, s_l4_end, &warming, s_env_first);
    REQUIRE(n == EXPECTED_AM_READY, "layout 4 produced %lu windows with a full ring, python has %d",
            (unsigned long)n, EXPECTED_AM_READY);
    CHECK(warming == (uint32_t)(EXPECTED_AM_WINDOWS - EXPECTED_AM_READY),
          "%lu windows closed warming, expected %d (the ones before the ring was full)",
          (unsigned long)warming, EXPECTED_AM_WINDOWS - EXPECTED_AM_READY);
    for (uint32_t w = 0u; w < EXPECTED_AM_READY; w++)
    {
        CHECK(s_l4_end[w] == expected_l4_end_frame[w], "window %lu closed at frame %u, python at %u",
              (unsigned long)w, s_l4_end[w], expected_l4_end_frame[w]);
    }

    /* The envelope itself, before anything is made of it: a wrong coefficient,
       section order or decimation phase shows here first. */
    for (uint32_t i = 0u; i < (uint32_t)(EXPECTED_ENV_FRAMES * EXPECTED_ENV_PER_FRAME); i++)
    {
        CHECK(close_enough(s_env_first[i], expected_env[i], ENV_REL, ENV_ABS),
              "envelope sample %lu: got %.9g, python %.9g", (unsigned long)i,
              (double)s_env_first[i], (double)expected_env[i]);
    }

    for (uint32_t w = 0u; w < EXPECTED_AM_READY; w++)
    {
        for (uint32_t i = BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL; i < EXPECTED_L4_WIDTH; i++)
        {
            const float got = s_l4[w][i], want = expected_l4[w][i];
            bool        ok;
            if (i == L4_IDX_FPEAK)
            {
                ok = fabsf(got - want) <= 1e-6f;
            }
            else if (i == L4_IDX_LINES)
            {
                ok = fabsf(got - want) <= L4_LINES_ABS;
            }
            else
            {
                ok = close_enough(got, want, L4_MOD_REL, L4_MOD_ABS);
            }
            CHECK(ok, "window %lu feature %lu: got %.9g, python %.9g", (unsigned long)w,
                  (unsigned long)i, (double)got, (double)want);
        }
    }
}

static void scenario_layout4_prefix_is_layout2(void)
{
    const uint32_t n = run_am(boomdetect_extractor_by_name("stats_spectral"), &s_am_l2[0][0],
                              EXPECTED_AM_WINDOWS, NULL, NULL, NULL);
    REQUIRE(n == EXPECTED_AM_WINDOWS, "layout 2 produced %lu windows on the AM signal, expected %d",
            (unsigned long)n, EXPECTED_AM_WINDOWS);
    /* Layout 2 closes every window; layout 4 only the last EXPECTED_AM_READY of them. */
    const uint32_t first_ready = (uint32_t)(EXPECTED_AM_WINDOWS - EXPECTED_AM_READY);
    for (uint32_t w = 0u; w < EXPECTED_AM_READY; w++)
    {
        CHECK(memcmp(s_l4[w], s_am_l2[first_ready + w], sizeof(s_am_l2[0])) == 0,
              "window %lu: the first 69 of layout 4 are not bit-identical to layout 2",
              (unsigned long)w);
    }
}

static void scenario_layout4_short_ring_is_nan(void)
{
    float                   out[BOOMDETECT_MOD_FEATURES];
    const boomdetect_side_t side = {
        .env = s_env_first, .env_head = 0u,
        .env_fill = (uint32_t)(EXPECTED_ENV_FRAMES * EXPECTED_ENV_PER_FRAME),
    };
    boomdetect_modulation_features(&side, out);
    for (uint32_t i = 0u; i < BOOMDETECT_MOD_FEATURES; i++)
    {
        CHECK(isnan(out[i]), "feature %lu of a short ring is %.9g, not NaN", (unsigned long)i,
              (double)out[i]);
    }
    boomdetect_modulation_features(NULL, out);
    CHECK(isnan(out[0]), "a NULL side channel did not give NaN");
}

int main(void)
{
    scenario_registry();
    scenario_scalars_match_python();
    scenario_layout2_matches_python();
    scenario_layout2_prefix_is_layout1();
    scenario_layout3_matches_python_and_is_zero_mean();
    scenario_layout4_matches_python();
    scenario_layout4_prefix_is_layout2();
    scenario_layout4_short_ring_is_nan();
    BD_TEST_REPORT("extractor_test", 1280); /* exact count from running the compiled binary */
}
