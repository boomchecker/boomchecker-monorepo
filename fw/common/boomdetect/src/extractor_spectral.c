/**
 * @file extractor_spectral.c
 * @brief Layout 2: the MFCC statistics plus what the MFCC throws away.
 *
 *   [ 0..51 ]  mean, std, dmean, cmax x 13 MFCC       (identical to layout 1)
 *   [52..59 ]  mean over the window of the 8 spectral scalars
 *   [60..67 ]  population std over the window of the same 8
 *   [  68   ]  log-mel flux: mean |logmel[f] - logmel[f-1]| over frames and bands
 *
 * Specified by features.stats_spectral() in the training package.
 */
#include "extractors.h"

#include <math.h>

void boomdetect_aggregate_stats(const float *frames, uint32_t nframes, uint32_t stride,
                                uint32_t coeffs, float *out)
{
    for (uint32_t c = 0u; c < coeffs; c++)
    {
        float sum = 0.0f;
        float mx = frames[c];
        for (uint32_t f = 0u; f < nframes; f++)
        {
            float v = frames[f * stride + c];
            sum += v;
            if (v > mx)
            {
                mx = v;
            }
        }
        float mean = sum / (float)nframes;
        out[c] = mean;

        float sq = 0.0f;
        float dsum = 0.0f;
        for (uint32_t f = 0u; f < nframes; f++)
        {
            float v = frames[f * stride + c];
            float d = v - mean;
            sq += d * d;
            if (f > 0u)
            {
                dsum += fabsf(v - frames[(f - 1u) * stride + c]);
            }
        }
        out[c + coeffs] = sqrtf(sq / (float)nframes);
        out[c + 2u * coeffs] = (nframes > 1u) ? (dsum / (float)(nframes - 1u)) : 0.0f;
        out[c + 3u * coeffs] = mx - mean;
    }
}

static void spectral_extract(void *ctx, const float *frames, uint32_t nframes, uint32_t stride,
                             float *out)
{
    (void)ctx;
    boomdetect_aggregate_stats(frames, nframes, stride, BOOMDETECT_MFCC_COEFFS, out);

    float *smean = out + 4u * BOOMDETECT_MFCC_COEFFS;
    float *sstd  = smean + BOOMDETECT_FRAME_SCALARS;
    for (uint32_t s = 0u; s < BOOMDETECT_FRAME_SCALARS; s++)
    {
        float sum = 0.0f;
        for (uint32_t f = 0u; f < nframes; f++)
        {
            sum += frames[f * stride + BOOMDETECT_FRAME_SCALAR_OFF + s];
        }
        const float mean = sum / (float)nframes;
        float       sq   = 0.0f;
        for (uint32_t f = 0u; f < nframes; f++)
        {
            const float d = frames[f * stride + BOOMDETECT_FRAME_SCALAR_OFF + s] - mean;
            sq += d * d;
        }
        smean[s] = mean;
        sstd[s]  = sqrtf(sq / (float)nframes);
    }

    /* Flux over every (frame pair, band) cell, one mean - matching
       np.abs(np.diff(lm, axis=0)).mean(). */
    float flux = 0.0f;
    if (nframes > 1u)
    {
        float acc = 0.0f;
        for (uint32_t f = 1u; f < nframes; f++)
        {
            const float *cur  = frames + f * stride + BOOMDETECT_FRAME_LOGMEL_OFF;
            const float *prev = frames + (f - 1u) * stride + BOOMDETECT_FRAME_LOGMEL_OFF;
            for (uint32_t m = 0u; m < BOOMDETECT_MEL_FILTERS; m++)
            {
                acc += fabsf(cur[m] - prev[m]);
            }
        }
        flux = acc / (float)((nframes - 1u) * BOOMDETECT_MEL_FILTERS);
    }
    sstd[BOOMDETECT_FRAME_SCALARS] = flux;
}

const boomdetect_extractor_t boomdetect_extractor_stats_spectral = {
    .name       = "stats_spectral",
    .layout_id  = BOOMDETECT_LAYOUT_STATS_SPECTRAL,
    .n_features = (uint16_t)BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL,
    .extract    = spectral_extract,
    .ctx        = NULL,
};
