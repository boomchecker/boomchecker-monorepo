/**
 * @file extractor_stats.c
 * @brief Layout 1, the deployed extractor, and the extractor registry.
 *
 * [mean, std, dmean, cmax] per MFCC coefficient. The arithmetic is
 * boomdetect_aggregate_stats() in extractor_spectral.c, shared with layout 2
 * so the two agree bit for bit on their common prefix; it is unchanged from the
 * version that lived in boomdetect.c - the deployed model was selected against
 * exactly these numbers, and the fixtures pin them.
 */
#include "extractors.h"

#include <string.h>

static void stats_extract(void *ctx, const float *frames, uint32_t nframes, uint32_t stride,
                          float *out)
{
    (void)ctx;
    boomdetect_aggregate_stats(frames, nframes, stride, BOOMDETECT_MFCC_COEFFS, out);
}

const boomdetect_extractor_t boomdetect_extractor_stats = {
    .name       = "stats",
    .layout_id  = BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX,
    .n_features = (uint16_t)BOOMDETECT_FEATURE_COUNT,
    .extract    = stats_extract,
    .ctx        = NULL,
};

/* First entry is the default, same convention as the classifier registry. */
static const boomdetect_extractor_t *const s_extractors[] = {
    &boomdetect_extractor_stats,
    &boomdetect_extractor_stats_spectral,
    &boomdetect_extractor_logmel,
};

size_t boomdetect_extractor_count(void)
{
    return sizeof(s_extractors) / sizeof(s_extractors[0]);
}

const boomdetect_extractor_t *boomdetect_extractor_at(size_t idx)
{
    return (idx < boomdetect_extractor_count()) ? s_extractors[idx] : NULL;
}

const boomdetect_extractor_t *boomdetect_extractor_default(void)
{
    return s_extractors[0];
}

const boomdetect_extractor_t *boomdetect_extractor_by_name(const char *name)
{
    if (name == NULL)
    {
        return NULL;
    }
    for (size_t i = 0u; i < boomdetect_extractor_count(); i++)
    {
        if (strcmp(s_extractors[i]->name, name) == 0)
        {
            return s_extractors[i];
        }
    }
    return NULL;
}

const boomdetect_extractor_t *boomdetect_extractor_for_layout(uint16_t layout_id)
{
    for (size_t i = 0u; i < boomdetect_extractor_count(); i++)
    {
        if (s_extractors[i]->layout_id == layout_id)
        {
            return s_extractors[i];
        }
    }
    return NULL;
}
