/**
 * @file extractor_mod.c
 * @brief Layout 4: layout 2, then the modulation spectrum of the envelope ring.
 *
 *   [ 0..68 ]  layout 2, bit for bit (boomdetect_extract_stats_spectral)
 *   [  69   ]  mod_prom_max       strongest line in 50-400 Hz, dB / 20
 *   [  70   ]  mod_lines          share of the 50-400 Hz bins more than 6 dB up
 *   [  71   ]  mod_f_peak         that line's frequency / 400 Hz
 *   [  72   ]  mod_prom_2f        best prominence within +-2 bins of twice it, dB / 20
 *   [73..76 ]  mod_band_*         strongest line in 50-150 / 150-250 / 250-400 /
 *                                 400-800 Hz, dB / 20
 *   [  77   ]  mod_depth          envelope std over mean
 *   [  78   ]  mod_share_100_250  envelope power 100-250 Hz over 10-500 Hz
 *
 * Specified by features.modulation_stats() in the training package. The ring
 * (BOOMDETECT_ENV_RING samples of the 1 kHz envelope, 1.98 s) is read oldest
 * first in 12 segments of 512 every 128 samples; each loses its mean, is
 * Hann-windowed (numpy's symmetric window) and transformed, the power spectra
 * are averaged (Welch), and a bin's prominence is its 10 log10 power over the
 * mean of the 41 bins around it, the ends repeating their edge value. A
 * hovering drone puts one line at its blade-pass rate (170-185 Hz for a
 * Phantom 4) with a harmonic at twice it; backgrounds have none.
 *
 * Float32 where the Python runs float64: the FFT is CMSIS's, log10f is the
 * libm one, and tests/vectors/extractor_mod_expected.h states the tolerance
 * that leaves. Two outputs are discrete - the peak bin and the line count -
 * and can move one step on a near-tie, which the fixture signal avoids.
 */
#include "extractors.h"

#include "arm_math.h"

#include <math.h>
#include <string.h>

#define MOD_SEG       512u
#define MOD_HOP       128u
#define MOD_NSEG      ((BOOMDETECT_ENV_RING - MOD_SEG) / MOD_HOP + 1u) /* 12 */
#define MOD_BINS      (MOD_SEG / 2u + 1u)                               /* 257 */
#define MOD_BIN_HZ    (1000.0f / (float)MOD_SEG) /* 1.953125, exact in float32 */
#define MOD_BASE_HALF 20u                        /* +-20 bins: a 41-bin running mean */
#define MOD_LINE_DB   6.0f
#define MOD_EPS       1.0e-20f
#define MOD_TWO_PI    6.283185307179586

_Static_assert(MOD_NSEG == 12u, "features.py averages twelve segments over the ring");

/* All static: the superloop's stack is small. The detector runs one window at
   a time, so one set serves every detector, like the MFCC instance in
   mfcc_processor.c. */
static arm_rfft_fast_instance_f32 s_rfft;
static bool                       s_ready = false;
static float                      s_hann[MOD_SEG];
static float                      s_env[BOOMDETECT_ENV_RING];
static float                      s_seg[MOD_SEG];
static float                      s_spec[MOD_SEG];
static float                      s_p[MOD_BINS];
static float                      s_logp[MOD_BINS];
static float                      s_prom[MOD_BINS];

static bool ensure_tables(void)
{
    if (!s_ready)
    {
        if (arm_rfft_fast_init_f32(&s_rfft, (uint16_t)MOD_SEG) != ARM_MATH_SUCCESS)
        {
            return false;
        }
        /* np.hanning(N): 0.5 - 0.5 cos(2 pi n / (N - 1)) - the symmetric one,
           not scipy's periodic 'hann'. In double so that the table lands on
           numpy's values to float32 rounding. */
        for (uint32_t n = 0u; n < MOD_SEG; n++)
        {
            s_hann[n] = (float)(0.5 - 0.5 * cos(MOD_TWO_PI * (double)n / (double)(MOD_SEG - 1u)));
        }
        s_ready = true;
    }
    return true;
}

/* Bins [k0, k1) whose frequency f satisfies lo <= f < hi, like features._band. */
static void band(float lo_hz, float hi_hz, uint32_t *k0, uint32_t *k1)
{
    uint32_t a = 0u;
    while (a < MOD_BINS && (float)a * MOD_BIN_HZ < lo_hz)
    {
        a++;
    }
    uint32_t b = a;
    while (b < MOD_BINS && (float)b * MOD_BIN_HZ < hi_hz)
    {
        b++;
    }
    *k0 = a;
    *k1 = b;
}

static float band_max(const float *v, uint32_t k0, uint32_t k1)
{
    float mx = v[k0];
    for (uint32_t k = k0 + 1u; k < k1; k++)
    {
        if (v[k] > mx)
        {
            mx = v[k];
        }
    }
    return mx;
}

static float band_sum(const float *v, uint32_t k0, uint32_t k1)
{
    float s = 0.0f;
    for (uint32_t k = k0; k < k1; k++)
    {
        s += v[k];
    }
    return s;
}

void boomdetect_modulation_features(const boomdetect_side_t *side, float *out)
{
    if (side == NULL || side->env == NULL || side->env_fill < BOOMDETECT_ENV_RING ||
        !ensure_tables())
    {
        for (uint32_t i = 0u; i < BOOMDETECT_MOD_FEATURES; i++)
        {
            out[i] = NAN;
        }
        return;
    }

    /* Oldest sample first, as the Python reads its env rows in time order. */
    for (uint32_t i = 0u; i < BOOMDETECT_ENV_RING; i++)
    {
        s_env[i] = side->env[(side->env_head + i) % BOOMDETECT_ENV_RING];
    }

    /* Welch: segment minus its mean, Hann, |FFT|^2 summed, then averaged. */
    memset(s_p, 0, sizeof(s_p));
    for (uint32_t s = 0u; s < MOD_NSEG; s++)
    {
        const float *seg  = s_env + s * MOD_HOP;
        float        mean = 0.0f;
        for (uint32_t n = 0u; n < MOD_SEG; n++)
        {
            mean += seg[n];
        }
        mean /= (float)MOD_SEG;
        for (uint32_t n = 0u; n < MOD_SEG; n++)
        {
            s_seg[n] = (seg[n] - mean) * s_hann[n];
        }
        arm_rfft_fast_f32(&s_rfft, s_seg, s_spec, 0u);
        /* CMSIS packs the real Nyquist value into the imaginary slot of DC. */
        s_p[0] += s_spec[0] * s_spec[0];
        s_p[MOD_BINS - 1u] += s_spec[1] * s_spec[1];
        for (uint32_t k = 1u; k < MOD_BINS - 1u; k++)
        {
            const float re = s_spec[2u * k];
            const float im = s_spec[2u * k + 1u];
            s_p[k] += re * re + im * im;
        }
    }
    for (uint32_t k = 0u; k < MOD_BINS; k++)
    {
        s_p[k]    = s_p[k] / (float)MOD_NSEG + MOD_EPS;
        s_logp[k] = 10.0f * log10f(s_p[k]);
    }

    /* Prominence over the local baseline: the mean of the 41 bins around k,
       indices clamped to the ends (np.pad mode="edge"). */
    for (uint32_t k = 0u; k < MOD_BINS; k++)
    {
        float acc = 0.0f;
        for (int32_t j = (int32_t)k - (int32_t)MOD_BASE_HALF; j <= (int32_t)k + (int32_t)MOD_BASE_HALF;
             j++)
        {
            const int32_t c = (j < 0) ? 0 : ((j >= (int32_t)MOD_BINS) ? (int32_t)MOD_BINS - 1 : j);
            acc += s_logp[c];
        }
        s_prom[k] = s_logp[k] - acc / (float)(2u * MOD_BASE_HALF + 1u);
    }

    /* The strongest line in 50-400 Hz: value, how many bins are lines, where. */
    uint32_t m0, m1;
    band(50.0f, 400.0f, &m0, &m1);
    uint32_t kpk  = m0;
    float    pmax = s_prom[m0];
    uint32_t lines = 0u;
    for (uint32_t k = m0; k < m1; k++)
    {
        if (s_prom[k] > pmax)
        {
            pmax = s_prom[k];
            kpk  = k;
        }
        if (s_prom[k] > MOD_LINE_DB)
        {
            lines++;
        }
    }
    out[0] = pmax / 20.0f;
    out[1] = (float)lines / (float)(m1 - m0);
    out[2] = ((float)kpk * MOD_BIN_HZ) / 400.0f;

    /* The harmonic: round(2 f / bin) is exactly twice the peak bin. */
    const uint32_t k2  = 2u * kpk;
    const uint32_t lo2 = (k2 >= 2u) ? (k2 - 2u) : 0u;
    const uint32_t hi2 = (k2 + 3u < MOD_BINS) ? (k2 + 3u) : MOD_BINS;
    out[3]             = (hi2 > lo2) ? (band_max(s_prom, lo2, hi2) / 20.0f) : 0.0f;

    static const float bands_hz[4][2] = {
        { 50.0f, 150.0f }, { 150.0f, 250.0f }, { 250.0f, 400.0f }, { 400.0f, 800.0f }
    };
    for (uint32_t j = 0u; j < 4u; j++)
    {
        uint32_t b0, b1;
        band(bands_hz[j][0], bands_hz[j][1], &b0, &b1);
        out[4u + j] = band_max(s_prom, b0, b1) / 20.0f;
    }

    /* Depth: population std of the envelope over its mean. */
    float sum = 0.0f;
    for (uint32_t i = 0u; i < BOOMDETECT_ENV_RING; i++)
    {
        sum += s_env[i];
    }
    const float mean = sum / (float)BOOMDETECT_ENV_RING;
    float       sq   = 0.0f;
    for (uint32_t i = 0u; i < BOOMDETECT_ENV_RING; i++)
    {
        const float d = s_env[i] - mean;
        sq += d * d;
    }
    out[8] = (mean > 0.0f) ? (sqrtf(sq / (float)BOOMDETECT_ENV_RING) / (mean + 1.0e-9f)) : 0.0f;

    /* Share of the envelope power that sits where a drone's modulation does. */
    uint32_t t0, t1, h0, h1;
    band(10.0f, 500.0f, &t0, &t1);
    band(100.0f, 250.0f, &h0, &h1);
    const float total = band_sum(s_p, t0, t1);
    out[9]            = (total > 0.0f) ? (band_sum(s_p, h0, h1) / total) : 0.0f;
}

static bool mod_prepare(void *ctx)
{
    (void)ctx;
    return ensure_tables();
}

static void mod_extract(void *ctx, const float *frames, uint32_t nframes, uint32_t stride,
                        const boomdetect_side_t *side, float *out)
{
    (void)ctx;
    boomdetect_extract_stats_spectral(frames, nframes, stride, out);
    boomdetect_modulation_features(side, out + BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL);
}

const boomdetect_extractor_t boomdetect_extractor_stats_spectral_mod = {
    .name         = "stats_spectral_mod",
    .layout_id    = BOOMDETECT_LAYOUT_STATS_SPECTRAL_MOD,
    .n_features   = (uint16_t)BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL_MOD,
    .env_required = (uint16_t)BOOMDETECT_ENV_RING,
    .prepare      = mod_prepare,
    .extract      = mod_extract,
    .ctx          = NULL,
};
