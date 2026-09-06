/**
 * @file extractor_logmel.c
 * @brief Layout 3: the log-mel patch a CNN reads.
 *
 * nframes x 20 values, frame-major, minus the mean of the whole patch. The
 * subtraction is the level invariance: a gain change adds a constant to every
 * log-mel value and the patch does not move. Rows for frames the window did
 * not have (a shorter accum_frames) are zero, so the width is fixed.
 *
 * Specified by features.logmel_patch() in the training package.
 */
#include "extractors.h"

#include <string.h>

static void logmel_extract(void *ctx, const float *frames, uint32_t nframes, uint32_t stride,
                           float *out)
{
    (void)ctx;
    const uint32_t rows = (nframes < BOOMDETECT_ACCUM_FRAMES_MAX) ? nframes : BOOMDETECT_ACCUM_FRAMES_MAX;
    const uint32_t n    = rows * BOOMDETECT_MEL_FILTERS;

    float sum = 0.0f;
    for (uint32_t f = 0u; f < rows; f++)
    {
        const float *lm = frames + f * stride + BOOMDETECT_FRAME_LOGMEL_OFF;
        for (uint32_t m = 0u; m < BOOMDETECT_MEL_FILTERS; m++)
        {
            const float v = lm[m];
            out[f * BOOMDETECT_MEL_FILTERS + m] = v;
            sum += v;
        }
    }
    const float mean = (n > 0u) ? (sum / (float)n) : 0.0f;
    for (uint32_t i = 0u; i < n; i++)
    {
        out[i] -= mean;
    }
    if (n < BOOMDETECT_FEATURE_COUNT_LOGMEL)
    {
        memset(out + n, 0, (BOOMDETECT_FEATURE_COUNT_LOGMEL - n) * sizeof(float));
    }
}

const boomdetect_extractor_t boomdetect_extractor_logmel = {
    .name       = "logmel",
    .layout_id  = BOOMDETECT_LAYOUT_LOGMEL,
    .n_features = (uint16_t)BOOMDETECT_FEATURE_COUNT_LOGMEL,
    .extract    = logmel_extract,
    .ctx        = NULL,
};
