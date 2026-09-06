/**
 * @file boomdetect.c
 * @brief Detection pipeline (see boomdetect.h).
 *
 * Lifted from fw/bom-stm32node/Core/Src/detector.c without touching the
 * arithmetic. The order of operations is load-bearing: the deployed model was
 * selected against the numbers this exact sequence produces, and
 * tests/vectors/selftest_expected.txt pins them.
 */

#include "boomdetect.h"

#include "arm_math.h"
#include "mfcc_processor.h"
#include "classifier.h"

#include <math.h>
#include <string.h>

/* The MFCC tables are global to the CMSIS instance, so initialise them once
   however many detectors exist. */
static bool s_mfcc_ready = false;

/* Aggregate BOOMDETECT_ACCUM_FRAMES MFCC frames into
     [mean(13), std(13), dmean(13), cmax(13)]
   population std (matching numpy's default, which is what the training used);
   dmean = mean absolute frame-to-frame delta, which is what carries propeller
   modulation; cmax = max - mean, a peakiness measure that does not move with
   level.

   All four are computed even though the linear SVM models only read the first
   26: computing the full vector is what lets one pipeline feed either family. */
static void aggregate(const float *frames, float *out)
{
    for (uint32_t c = 0u; c < NUM_MFCC_COEFFS; c++)
    {
        float sum = 0.0f;
        float mx = frames[c];
        for (uint32_t f = 0u; f < BOOMDETECT_ACCUM_FRAMES; f++)
        {
            float v = frames[f * NUM_MFCC_COEFFS + c];
            sum += v;
            if (v > mx)
            {
                mx = v;
            }
        }
        float mean = sum / (float)BOOMDETECT_ACCUM_FRAMES;
        out[c] = mean;

        float sq = 0.0f;
        float dsum = 0.0f;
        for (uint32_t f = 0u; f < BOOMDETECT_ACCUM_FRAMES; f++)
        {
            float v = frames[f * NUM_MFCC_COEFFS + c];
            float d = v - mean;
            sq += d * d;
            if (f > 0u)
            {
                dsum += fabsf(v - frames[(f - 1u) * NUM_MFCC_COEFFS + c]);
            }
        }
        out[c + NUM_MFCC_COEFFS] = sqrtf(sq / (float)BOOMDETECT_ACCUM_FRAMES);
        out[c + 2u * NUM_MFCC_COEFFS] = dsum / (float)(BOOMDETECT_ACCUM_FRAMES - 1u);
        out[c + 3u * NUM_MFCC_COEFFS] = mx - mean;
    }
}

bool boomdetect_init(boomdetect_t *d, const boomdetect_config_t *cfg)
{
    if (d == NULL || cfg == NULL || cfg->decimation == 0u)
    {
        return false;
    }

    const classifier_t *model = (cfg->classifier != NULL) ? cfg->classifier
                                                          : classifier_default();

    /* Refuse a model that does not fit rather than reading past the feature
       vector. Each model asserts its own dimensions at compile time, but a
       caller can hand over any entry it likes, so check here too. */
    if (model->decide == NULL || model->layout_id != BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX ||
        (uint32_t)model->feature_offset + model->n_features > DET_FEATURE_COUNT)
    {
        return false;
    }

    if (!s_mfcc_ready)
    {
        if (mfcc_init() != ARM_MATH_SUCCESS)
        {
            return false;
        }
        s_mfcc_ready = true;
    }

    memset(d, 0, sizeof(*d));
    d->cfg = *cfg;
    d->cfg.classifier = model;
    return true;
}

void boomdetect_push(boomdetect_t *d, const int16_t *pcm, size_t n)
{
    /* Decimate straight into the FIFO, carrying the phase across calls: a
       block is not generally a multiple of the decimation factor, so restarting
       at zero each time would repeat or skip a sample at every boundary. */
    size_t i = d->dec_phase;
    for (; i < n; i += d->cfg.decimation)
    {
        if (d->avail >= BOOMDETECT_RING_LEN)
        {
            d->dropped++;
            continue;
        }
        d->ring[d->w_idx] = (float)pcm[i] * (1.0f / 32768.0f);
        d->w_idx = (d->w_idx + 1u) % BOOMDETECT_RING_LEN;
        d->avail++;
    }
    d->dec_phase = (uint32_t)(i - n);
}

bool boomdetect_step(boomdetect_t *d, boomdetect_event_t *out)
{
    if (d->avail < WINDOW_SIZE)
    {
        return false;
    }

    uint32_t idx = d->r_idx;
    for (uint32_t k = 0u; k < WINDOW_SIZE; k++)
    {
        d->frame[k] = d->ring[idx];
        idx = (idx + 1u) % BOOMDETECT_RING_LEN;
    }

    float rms = 0.0f;
    arm_rms_f32(d->frame, WINDOW_SIZE, &rms);

    memset(out, 0, sizeof(*out));
    out->frame_index = d->frame_index;
    out->rms = rms;

    const float squelch = (float)d->cfg.squelch_milli / 1000.0f;
    if (rms < squelch)
    {
        /* Too quiet: throw away whatever the window had accumulated. This is
           the firmware's gating policy and it differs from how the training
           windows were selected - see the note in boomdetect.h. */
        d->accum = 0u;
        out->squelched = true;
    }
    else
    {
        if (d->accum == 0u)
        {
            d->window_start_frame = d->frame_index;
        }

        /* The MFCC destroys its input, which is why d->frame is a copy of the
           ring rather than a view into it. */
        d->last_mfcc_slot = d->accum;
        mfcc_process(d->frame, &d->mfccs[d->accum * NUM_MFCC_COEFFS]);
        d->accum++;

        if (d->accum >= BOOMDETECT_ACCUM_FRAMES)
        {
            aggregate(d->mfccs, d->features);
            const classifier_t *model = d->cfg.classifier;
            /* The entry declares where its slice starts, so decide() reads from
               index 0 and needs to know nothing about the layout. */
            const float decision = model->decide(model->ctx,
                                                 d->features + model->feature_offset);
            const float threshold = (float)d->cfg.thr_milli / 1000.0f;

            out->window_complete = true;
            out->window_start_frame = d->window_start_frame;
            out->decision = decision;
            out->is_drone = (decision >= threshold);

            d->windows++;
            if (out->is_drone)
            {
                d->drones++;
            }
            d->accum = 0u;
        }
    }

    out->accum = d->accum;

    d->r_idx = (d->r_idx + BOOMDETECT_HOP) % BOOMDETECT_RING_LEN;
    d->avail -= BOOMDETECT_HOP;
    d->frame_index++;
    return true;
}

uint32_t boomdetect_dropped(const boomdetect_t *d)
{
    return d->dropped;
}

void boomdetect_counts(const boomdetect_t *d, uint32_t *windows, uint32_t *drones)
{
    if (windows != NULL)
    {
        *windows = d->windows;
    }
    if (drones != NULL)
    {
        *drones = d->drones;
    }
}

const float *boomdetect_last_mfcc(const boomdetect_t *d)
{
    return &d->mfccs[d->last_mfcc_slot * NUM_MFCC_COEFFS];
}

const float *boomdetect_last_features(const boomdetect_t *d)
{
    return d->features;
}
