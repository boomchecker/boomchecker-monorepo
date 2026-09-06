/**
 * @file model_mlp_v6.c
 * @brief The deployed model: a 51 -> 32 -> 1 ReLU MLP.
 *
 * Selected by pick_champion.py as the winner of the bebop/membo alarm criterion
 * with zero two-window false alarms, at an operating point of +7.25.
 *
 * default_thr_milli below is 15000, not 7250, because +7.25 was chosen offline
 * against recordings and this was the first time the model met a node with a
 * working microphone: it fired on 23 of 396 windows of ordinary office noise,
 * peaking at 12.05, and a second three-minute campaign at 15.0 saw none.
 *
 * What that number is not: it was measured against ambient noise with no drone
 * present, so the sensitivity it gives up is unquantified, and the spread
 * between sessions (one peaked at 12.05, another at 2.77, same board and
 * firmware) is wider than the change it justified. A field default chosen to
 * stop crying wolf, not an operating point swept on labelled data.
 *
 * The decision is a raw logit, not a probability. A threshold that looks
 * sensible for a probability (0.5, say) is nowhere near this model's operating
 * range, which is why the threshold belongs to the model rather than to the
 * detector.
 */
#include "classifier.h"
#include "dsp_config.h" /* BOOMDETECT_FEATURE_COUNT, for the assert below */
#include "models.h"

#include "arm_math.h"
#include "mlp_model_data_v6.h"

/* Feature 0 is the mean of MFCC coefficient 0, which tracks absolute level.
   This family is trained to be gain-invariant, so it starts at 1. */
#define MLP_V6_OFFSET 1u

_Static_assert(MLP_V6_OFFSET + MLP_NUM_INPUTS == BOOMDETECT_FEATURE_COUNT,
               "mlp_v6's slice does not cover the features the aggregate produces");

static float mlp_v6_decide(void *ctx, const float *features, uint16_t n)
{
    (void)n; /* declared MLP_NUM_INPUTS, static-asserted below against the layout */
    float32_t x[MLP_NUM_INPUTS];
    float32_t h[MLP_HIDDEN];

    (void)ctx;

    for (int i = 0; i < MLP_NUM_INPUTS; i++)
    {
        x[i] = (features[i] - mlp_scaler_mean[i]) * mlp_scaler_inv_std[i];
    }
    for (int j = 0; j < MLP_HIDDEN; j++)
    {
        float32_t acc = 0.0f;
        arm_dot_prod_f32(x, mlp_w1[j], MLP_NUM_INPUTS, &acc);
        acc += mlp_b1[j];
        h[j] = (acc > 0.0f) ? acc : 0.0f;
    }
    float32_t out = 0.0f;
    arm_dot_prod_f32(h, mlp_w2, MLP_HIDDEN, &out);
    return out + MLP_B2;
}

const classifier_t classifier_mlp_v6 = {
    .name              = "mlp_v6",
    .layout_id         = BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX,
    .n_features        = MLP_NUM_INPUTS,
    .feature_offset    = MLP_V6_OFFSET,
    .default_thr_milli = 15000,
    .decide            = mlp_v6_decide,
    .ctx               = NULL,
};
