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

/* Aggregate `nframes` MFCC frames into
     [mean(13), std(13), dmean(13), cmax(13)]
   population std (matching numpy's default, which is what the training used);
   dmean = mean absolute frame-to-frame delta, which is what carries propeller
   modulation; cmax = max - mean, a peakiness measure that does not move with
   level.

   All four are computed even though the linear SVM models only read the first
   26: computing the full vector is what lets one pipeline feed either family. */
static void aggregate(const float *frames, uint32_t nframes, float *out)
{
    for (uint32_t c = 0u; c < BOOMDETECT_MFCC_COEFFS; c++)
    {
        float sum = 0.0f;
        float mx = frames[c];
        for (uint32_t f = 0u; f < nframes; f++)
        {
            float v = frames[f * BOOMDETECT_MFCC_COEFFS + c];
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
            float v = frames[f * BOOMDETECT_MFCC_COEFFS + c];
            float d = v - mean;
            sq += d * d;
            if (f > 0u)
            {
                dsum += fabsf(v - frames[(f - 1u) * BOOMDETECT_MFCC_COEFFS + c]);
            }
        }
        out[c + BOOMDETECT_MFCC_COEFFS] = sqrtf(sq / (float)nframes);
        out[c + 2u * BOOMDETECT_MFCC_COEFFS] = dsum / (float)(nframes - 1u);
        out[c + 3u * BOOMDETECT_MFCC_COEFFS] = mx - mean;
    }
}

/* Median of up to BOOMDETECT_ACCUM_FRAMES values. Insertion sort on a copy: n
   is 14 at most, and sorting rms_hist in place would destroy the order. */
static float median_of(const float *v, uint32_t n)
{
    float tmp[BOOMDETECT_ACCUM_FRAMES];
    if (n == 0u || n > BOOMDETECT_ACCUM_FRAMES)
    {
        return 0.0f;
    }
    memcpy(tmp, v, n * sizeof(tmp[0]));
    for (uint32_t i = 1u; i < n; i++)
    {
        const float key = tmp[i];
        uint32_t    j   = i;
        while (j > 0u && tmp[j - 1u] > key)
        {
            tmp[j] = tmp[j - 1u];
            j--;
        }
        tmp[j] = key;
    }
    /* Even n takes the mean of the two middle values, which is what numpy does
       and therefore what the training pipeline's gate did. */
    return ((n % 2u) != 0u) ? tmp[n / 2u]
                            : (0.5f * (tmp[(n / 2u) - 1u] + tmp[n / 2u]));
}

bool boomdetect_init(boomdetect_t *d, const boomdetect_config_t *cfg)
{
    if (d == NULL || cfg == NULL || cfg->decimation == 0u)
    {
        return false;
    }
    /* accum_frames sizes nothing at run time - mfccs[] is dimensioned for the
       maximum - but a value above it would write past the slot array, and a hop
       above BOOMDETECT_WINDOW_SIZE would advance the FIFO further than the frame just
       read, silently skipping input. */
    if (cfg->accum_frames > BOOMDETECT_ACCUM_FRAMES || cfg->hop > BOOMDETECT_WINDOW_SIZE)
    {
        return false;
    }
    if (cfg->gate != BOOMDETECT_GATE_PER_FRAME && cfg->gate != BOOMDETECT_GATE_WINDOW_MEDIAN)
    {
        return false;
    }

    const classifier_t *model = (cfg->classifier != NULL) ? cfg->classifier
                                                          : classifier_default();

    /* Refuse a model that does not fit rather than reading past the feature
       vector. Each model asserts its own dimensions at compile time, but a
       caller can hand over any entry it likes, so check here too. */
    if (model->decide == NULL || model->layout_id != BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX ||
        (uint32_t)model->feature_offset + model->n_features > BOOMDETECT_FEATURE_COUNT)
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

    /* Fill the window policy in here rather than at every call site. 0 means
       "the firmware's value", so a caller that zero-initialises its config gets
       the shipped behaviour and every checked-in fixture still matches. */
    if (d->cfg.accum_frames == 0u)
    {
        d->cfg.accum_frames = BOOMDETECT_ACCUM_FRAMES;
    }
    if (d->cfg.hop == 0u)
    {
        d->cfg.hop = BOOMDETECT_HOP;
    }
    /* The operating point belongs to the model. Resolving it here is what stops
       each consumer reimplementing the lookup, and one of them forgetting is a
       detector that never fires with nothing to say so. */
    if (d->cfg.thr_milli == BOOMDETECT_THR_MODEL_DEFAULT)
    {
        d->cfg.thr_milli = model->default_thr_milli;
    }
    d->last_mfcc_slot = BOOMDETECT_NO_MFCC;
    return true;
}

size_t boomdetect_push(boomdetect_t *d, const int16_t *pcm, size_t n)
{
    /* A caller that ignored a failed init leaves decimation at 0, and the loop
       below would then never advance i: it would fill the FIFO and spin forever
       incrementing dropped. On the board that is a watchdog reset with no
       diagnostic, so refuse rather than hang. */
    if (d == NULL || pcm == NULL || d->cfg.decimation == 0u)
    {
        return 0u;
    }

    size_t stored = 0u;

    /* Decimate straight into the FIFO, carrying the phase across calls: a
       block is not generally a multiple of the decimation factor, so restarting
       at zero each time would repeat or skip a sample at every boundary. */
    size_t i = d->dec_phase;
    for (; i < n; i += d->cfg.decimation)
    {
        if (d->avail >= BOOMDETECT_RING_LEN)
        {
            d->dropped++;
            /* Latched, not reported here: the caller is mid-block and the frame
               that straddles the loss has not been built yet. The next frame
               carries the flag, which is what lets a replay harness discard
               exactly the affected window instead of the whole run. */
            d->gap_pending = true;
            continue;
        }
        d->ring[d->w_idx] = (float)pcm[i] * (1.0f / 32768.0f);
        d->w_idx = (d->w_idx + 1u) % BOOMDETECT_RING_LEN;
        d->avail++;
        stored++;
    }
    d->dec_phase = (uint32_t)(i - n);
    return stored;
}

bool boomdetect_step(boomdetect_t *d, boomdetect_event_t *out)
{
    if (d == NULL || out == NULL || d->cfg.decimation == 0u || d->cfg.hop == 0u)
    {
        return false;
    }
    if (d->avail < BOOMDETECT_WINDOW_SIZE)
    {
        return false;
    }

    uint32_t idx = d->r_idx;
    for (uint32_t k = 0u; k < BOOMDETECT_WINDOW_SIZE; k++)
    {
        d->frame[k] = d->ring[idx];
        idx = (idx + 1u) % BOOMDETECT_RING_LEN;
    }

    float rms = 0.0f;
    arm_rms_f32(d->frame, BOOMDETECT_WINDOW_SIZE, &rms);

    memset(out, 0, sizeof(*out));
    out->frame_index = d->frame_index;
    out->rms = rms;
    out->gap = d->gap_pending;
    d->gap_pending = false;
    /* Not a number until a window completes. Zero would be a perfectly
       plausible logit, and a caller reading it without checking `complete`
       would get "classified, not a drone" out of a frame that was never
       classified at all. */
    out->window.decision = NAN;

    const float    squelch = (float)d->cfg.squelch_milli / 1000.0f;
    const uint32_t accum_target = d->cfg.accum_frames;

    /* Under the per-frame gate a quiet frame is rejected outright and resets
       the accumulation. Under the window-median gate every frame is kept and
       the whole window is judged at the end, which is what the training
       pipeline did; the two are not interchangeable, and which one produced a
       given operating point is exactly the skew this config exists to measure. */
    const bool reject_frame =
        (d->cfg.gate == BOOMDETECT_GATE_PER_FRAME) && (rms < squelch);

    if (reject_frame)
    {
        d->accum = 0u;
        out->squelched = true;
        /* No MFCC was computed for this frame. Without this the accessor would
           keep returning the previously accepted frame's coefficients, and a
           parity harness logging one vector per step would record that vector
           twice and report a mismatch that is its own. */
        d->last_mfcc_slot = BOOMDETECT_NO_MFCC;
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
        mfcc_process(d->frame, &d->mfccs[d->accum * BOOMDETECT_MFCC_COEFFS]);
        d->rms_hist[d->accum] = rms;
        d->accum++;

        if (d->accum >= accum_target)
        {
            const bool keep =
                (d->cfg.gate != BOOMDETECT_GATE_WINDOW_MEDIAN) ||
                (median_of(d->rms_hist, d->accum) >= squelch);

            if (keep)
            {
                aggregate(d->mfccs, d->accum, d->features);
                const classifier_t *model = d->cfg.classifier;
                /* The entry declares where its slice starts and how wide it is,
                   so decide() reads from index 0 and needs to know nothing about
                   the layout. Passing the width means a model reading past its
                   own slice is a bug it can catch rather than a silent read of
                   the next family's features. */
                const float decision = model->decide(model->ctx,
                                                     d->features + model->feature_offset,
                                                     model->n_features);
                const float threshold = (float)d->cfg.thr_milli / 1000.0f;

                out->window.complete    = true;
                out->window.start_frame = d->window_start_frame;
                out->window.end_frame   = d->frame_index;
                out->window.decision    = decision;
                out->window.is_drone    = (decision >= threshold);

                d->windows++;
                if (out->window.is_drone)
                {
                    d->drones++;
                }
            }
            else
            {
                /* Dropped as a whole, which is the point of this gate. Reported
                   as squelched so a caller counting gated frames sees it. */
                out->squelched = true;
            }
            d->accum = 0u;
        }
    }

    out->accum = d->accum;

    d->r_idx = (d->r_idx + d->cfg.hop) % BOOMDETECT_RING_LEN;
    d->avail -= d->cfg.hop;
    d->frame_index++;
    return true;
}

uint32_t boomdetect_dropped(const boomdetect_t *d)
{
    return d->dropped;
}

uint32_t boomdetect_frame_index(const boomdetect_t *d)
{
    return d->frame_index;
}

uint32_t boomdetect_frame_to_ms(const boomdetect_t *d, uint32_t frame_index)
{
    /* Integer throughout: the firmware has no %f, and a float here would round
       a timestamp the caller then prints as an exact millisecond. */
    const uint32_t rate = (uint32_t)BOOMDETECT_SAMPLE_RATE_HZ;
    if (rate == 0u)
    {
        return 0u;
    }
    return (uint32_t)(((uint64_t)frame_index * d->cfg.hop * 1000u) / rate);
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
    if (d->last_mfcc_slot >= d->cfg.accum_frames)
    {
        return NULL;
    }
    return &d->mfccs[d->last_mfcc_slot * BOOMDETECT_MFCC_COEFFS];
}

const float *boomdetect_last_features(const boomdetect_t *d)
{
    /* Zeroed until a window completes, which is a plausible-looking feature
       vector rather than an obvious absence - so say nothing instead. */
    return (d->windows == 0u) ? NULL : d->features;
}
