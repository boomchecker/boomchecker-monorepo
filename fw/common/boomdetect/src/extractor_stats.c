/**
 * @file extractor_stats.c
 * @brief The one feature extractor this build carries.
 *
 * [mean, std, dmean, cmax] per MFCC coefficient. Lifted out of boomdetect.c
 * unchanged - the deployed model was selected against exactly these numbers, and
 * the fixtures pin them - so that the pipeline asks a registry for its layout
 * instead of hardcoding one. See include/extractor.h for why now.
 */
#include "extractor.h"

#include "dsp_config.h"

#include <math.h>
#include <string.h>

/* Aggregate `nframes` MFCC frames into
     [mean(13), std(13), dmean(13), cmax(13)]
   population std (matching numpy's default, which is what the training used);
   dmean = mean absolute frame-to-frame delta, which is what carries propeller
   modulation; cmax = max - mean, a peakiness measure that does not move with
   level.

   All four are computed even though the linear SVM models only read the first
   26: computing the full vector is what lets one pipeline feed either family. */
static void aggregate(const float *frames, uint32_t nframes, uint32_t coeffs,
                      float *out)
{
    for (uint32_t c = 0u; c < coeffs; c++)
    {
        float sum = 0.0f;
        float mx = frames[c];
        for (uint32_t f = 0u; f < nframes; f++)
        {
            float v = frames[f * coeffs + c];
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
            float v = frames[f * coeffs + c];
            float d = v - mean;
            sq += d * d;
            if (f > 0u)
            {
                dsum += fabsf(v - frames[(f - 1u) * coeffs + c]);
            }
        }
        out[c + coeffs] = sqrtf(sq / (float)nframes);
        out[c + 2u * coeffs] = dsum / (float)(nframes - 1u);
        out[c + 3u * coeffs] = mx - mean;
    }
}

static void stats_extract(void *ctx, const float *frames, uint32_t nframes, uint32_t coeffs,
                          float *out)
{
    (void)ctx;
    aggregate(frames, nframes, coeffs, out);
}

static const boomdetect_extractor_t s_stats = {
    .name       = "stats",
    .layout_id  = BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX,
    .n_features = (uint16_t)BOOMDETECT_FEATURE_COUNT,
    .extract    = stats_extract,
    .ctx        = NULL,
};

/* First entry is the default, same convention as the classifier registry. */
static const boomdetect_extractor_t *const s_extractors[] = { &s_stats };

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
