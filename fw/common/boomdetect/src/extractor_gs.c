/**
 * @file extractor_gs.c
 * @brief The gammatone hybrids: a spectrogram patch, and layout 4 or 8 beside it.
 *
 *   703  hybrid8_gs1k    [ 14 x 64 patch | layout 8 (89) ]   985
 *   903  hybrid831_gs1k  [ 31 x 64 patch | layout 8 (89) ]  2073
 *   603  hybrid31_gs1k   [ 31 x 64 patch | layout 4 (79) ]  2063
 *
 * As evaluate.window_features() in the training package: the patch is the newest
 * T rows of the gammatone ring, oldest first, minus the patch mean. The aux block
 * is layout 4 or 8 as their own extractors make it, NaN in a modulation set while
 * the envelope ring is shorter than its span - the networks were trained on such
 * windows, so these extractors keep the ring fed without waiting for it. They do
 * wait for the patch, as training dropped windows reaching before the clip.
 */
#include "extractors.h"
#include "gammatone.h"

#include <math.h>

typedef void (*aux_fn)(const float *frames, uint32_t nframes, uint32_t stride,
                       const boomdetect_side_t *side, float *out);

typedef struct
{
    uint32_t t; /**< patch rows */
    aux_fn   aux;
} gs_hybrid_t;

static void gs_patch(const boomdetect_side_t *side, uint32_t t, float *out)
{
    const uint32_t n = t * BOOMDETECT_GS_BANDS;
    if (side == NULL || side->gs == NULL || t > BOOMDETECT_GS_RING_ROWS || side->gs_fill < t)
    {
        for (uint32_t i = 0u; i < n; i++)
        {
            out[i] = NAN;
        }
        return;
    }
    /* Row sums in float, their total in double: the patch mean of up to 1984
       values of size ~20 would otherwise drift by more than the fixture allows. */
    const uint32_t first = side->gs_head + side->gs_fill - t;
    double         total = 0.0;
    for (uint32_t r = 0u; r < t; r++)
    {
        const float *row = side->gs + ((first + r) % BOOMDETECT_GS_RING_ROWS) * BOOMDETECT_GS_BANDS;
        float       *dst = out + r * BOOMDETECT_GS_BANDS;
        float        sum = 0.0f;
        for (uint32_t b = 0u; b < BOOMDETECT_GS_BANDS; b++)
        {
            dst[b] = row[b];
            sum += row[b];
        }
        total += (double)sum;
    }
    const float mean = (float)(total / (double)n);
    for (uint32_t i = 0u; i < n; i++)
    {
        out[i] -= mean;
    }
}

static bool gs_prepare(void *ctx)
{
    (void)ctx;
    return boomdetect_gs_prepare() && boomdetect_modulation_prepare();
}

static void gs_extract(void *ctx, const float *frames, uint32_t nframes, uint32_t stride,
                       const boomdetect_side_t *side, float *out)
{
    const gs_hybrid_t *h = (const gs_hybrid_t *)ctx;
    gs_patch(side, h->t, out);
    h->aux(frames, nframes, stride, side, out + h->t * BOOMDETECT_GS_BANDS);
}

static gs_hybrid_t s_h8_14 = {BOOMDETECT_GS_PATCH_SHORT, boomdetect_extract_stats_spectral_mod2s4s};
static gs_hybrid_t s_h8_31 = {BOOMDETECT_GS_PATCH_LONG, boomdetect_extract_stats_spectral_mod2s4s};
static gs_hybrid_t s_h4_31 = {BOOMDETECT_GS_PATCH_LONG, boomdetect_extract_stats_spectral_mod};

const boomdetect_extractor_t boomdetect_extractor_hybrid8_gs1k = {
    .name         = "hybrid8_gs1k",
    .layout_id    = BOOMDETECT_LAYOUT_HYBRID8_GS1K,
    .n_features   = (uint16_t)BOOMDETECT_FEATURE_COUNT_HYBRID8_GS1K,
    .env_feed     = true,
    .gs_required  = (uint16_t)BOOMDETECT_GS_PATCH_SHORT,
    .prepare      = gs_prepare,
    .extract      = gs_extract,
    .ctx          = &s_h8_14,
};

const boomdetect_extractor_t boomdetect_extractor_hybrid8_31_gs1k = {
    .name         = "hybrid831_gs1k",
    .layout_id    = BOOMDETECT_LAYOUT_HYBRID8_31_GS1K,
    .n_features   = (uint16_t)BOOMDETECT_FEATURE_COUNT_HYBRID8_31_GS1K,
    .env_feed     = true,
    .gs_required  = (uint16_t)BOOMDETECT_GS_PATCH_LONG,
    .prepare      = gs_prepare,
    .extract      = gs_extract,
    .ctx          = &s_h8_31,
};

const boomdetect_extractor_t boomdetect_extractor_hybrid31_gs1k = {
    .name         = "hybrid31_gs1k",
    .layout_id    = BOOMDETECT_LAYOUT_HYBRID31_GS1K,
    .n_features   = (uint16_t)BOOMDETECT_FEATURE_COUNT_HYBRID31_GS1K,
    .env_feed     = true,
    .gs_required  = (uint16_t)BOOMDETECT_GS_PATCH_LONG,
    .prepare      = gs_prepare,
    .extract      = gs_extract,
    .ctx          = &s_h4_31,
};
