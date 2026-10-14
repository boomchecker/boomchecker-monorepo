/**
 * @file frame_scalars.c
 * @brief Per-frame spectral scalars; see frame_scalars.h.
 *
 * Float32 throughout, because that is what the Cortex-M33's FPU does and a
 * double here would be software arithmetic on the board. The Python
 * specification runs in float64; the sums over 512 bins therefore agree to
 * about 1e-5 relative, and the parity fixture states that tolerance rather
 * than pretending to bit-exactness. Two values are discrete - the roll-off bin
 * and the comb's fundamental - and can land one step apart when the underlying
 * comparison is a near-tie; the fixture marks the windows where it is not.
 */
#include "frame_scalars.h"

#include <math.h>

#define N_BINS 513u
#define N_USED (N_BINS - 1u) /* bins 1..512 */
#define POWER_EPS 1.0e-12f
#define BIN_HZ 15.625f       /* 16000 / 1024 */
#define BIN_1K 64u           /* 1000 / 15.625 */
#define BIN_4K 256u          /* 4000 / 15.625 */
#define ROLLOFF_FRACTION 0.85f
#define N_HARMONICS 8u
#define F0_MAX_HZ 400.0f
#define NYQUIST_HZ 8000.0f

/* Comb fundamentals: 60 Hz to just under 400 Hz, 1/24 octave apart. Generated
   as float32 by training/boomdetect_train/features.py (f0_candidates) and
   pasted, so both sides interpolate at exactly the same frequencies. */
#define N_F0 66u
static const float s_f0[N_F0] = {
    60.0f, 61.7581329f, 63.5677872f, 65.4304657f, 67.3477249f, 69.3211594f,
    71.3524246f, 73.4432144f, 75.5952606f, 77.8103714f, 80.0903931f, 82.4372177f,
    84.8528137f, 87.3391876f, 89.8984222f, 92.5326462f, 95.2440643f, 98.0349274f,
    100.90757f, 103.864388f, 106.907845f, 110.040482f, 113.264915f, 116.583832f,
    120.0f, 123.516266f, 127.135574f, 130.860931f, 134.69545f, 138.642319f,
    142.704849f, 146.886429f, 151.190521f, 155.620743f, 160.180786f, 164.874435f,
    169.705627f, 174.678375f, 179.796844f, 185.065292f, 190.488129f, 196.069855f,
    201.81514f, 207.728775f, 213.815689f, 220.080963f, 226.529831f, 233.167664f,
    240.0f, 247.032532f, 254.271149f, 261.721863f, 269.3909f, 277.284637f,
    285.409698f, 293.772858f, 302.381042f, 311.241486f, 320.361572f, 329.748871f,
    339.411255f, 349.35675f, 359.593689f, 370.130585f, 380.976257f, 392.139709f,
};

/* Linear interpolation of the spectrum at `hz`; mirrors features._interp_mag. */
static float interp_mag(const float *mag, float hz)
{
    const float pos  = hz / BIN_HZ;
    float       fl   = floorf(pos);
    const float frac = pos - fl;
    int32_t     k    = (int32_t)fl;
    if (k < 0)
    {
        k = 0;
    }
    if (k > (int32_t)(N_BINS - 2u))
    {
        k = (int32_t)(N_BINS - 2u);
    }
    return mag[k] * (1.0f - frac) + mag[k + 1] * frac;
}

void boomdetect_frame_scalars(const float *mag, float *out)
{
    /* One pass for the power sums, magnitude sum, max and log-mean. The order
       of summation (ascending bin) is the same as numpy's, which keeps the
       float32/float64 gap to rounding rather than to association. */
    float total = 0.0f, hi = 0.0f, mid = 0.0f, weighted = 0.0f;
    float msum = 0.0f, mmax = 0.0f, logsum = 0.0f;
    for (uint32_t k = 1u; k < N_BINS; k++)
    {
        const float m = mag[k];
        const float p = m * m;
        total += p;
        weighted += (float)k * p;
        if (k >= BIN_4K)
        {
            hi += p;
        }
        else if (k >= BIN_1K)
        {
            mid += p;
        }
        msum += m;
        if (m > mmax)
        {
            mmax = m;
        }
        logsum += logf(p + POWER_EPS);
    }
    const float total_eps = total + POWER_EPS;
    const float msum_eps  = msum + POWER_EPS;

    out[BOOMDETECT_SCALAR_HI_RATIO]  = hi / total_eps;
    out[BOOMDETECT_SCALAR_MID_RATIO] = mid / total_eps;
    out[BOOMDETECT_SCALAR_CENTROID]  = (weighted / total_eps) * BIN_HZ / NYQUIST_HZ;
    out[BOOMDETECT_SCALAR_FLATNESS] =
        expf(logsum / (float)N_USED) / ((total / (float)N_USED) + POWER_EPS);

    /* Roll-off: the first bin at which the running sum reaches 85 % of the
       total (the total WITHOUT the epsilon, as numpy's cumsum[-1]). */
    const float target = ROLLOFF_FRACTION * total;
    float       acc    = 0.0f;
    uint32_t    roll   = N_USED;
    for (uint32_t k = 1u; k < N_BINS; k++)
    {
        acc += mag[k] * mag[k];
        if (acc >= target)
        {
            roll = k;
            break;
        }
    }
    out[BOOMDETECT_SCALAR_ROLLOFF] = (float)roll / (float)N_USED;
    out[BOOMDETECT_SCALAR_CREST]   = mmax / (msum_eps / (float)N_USED);

    /* Harmonic comb: the fundamental whose first eight harmonics collect the
       most magnitude. Strictly greater, so the first of two equal sums wins,
       as numpy's argmax does. */
    float    best    = 0.0f;
    uint32_t best_i  = 0u;
    for (uint32_t i = 0u; i < N_F0; i++)
    {
        float comb = 0.0f;
        for (uint32_t h = 1u; h <= N_HARMONICS; h++)
        {
            const float hz = (float)h * s_f0[i];
            if (hz > NYQUIST_HZ)
            {
                break;
            }
            comb += interp_mag(mag, hz);
        }
        if (comb > best)
        {
            best   = comb;
            best_i = i;
        }
    }
    out[BOOMDETECT_SCALAR_HARMONICITY] = (best > 0.0f) ? (best / msum_eps) : 0.0f;
    out[BOOMDETECT_SCALAR_F0]          = s_f0[best_i] / F0_MAX_HZ;
}
