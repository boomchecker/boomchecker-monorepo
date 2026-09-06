/**
 * @file model_mlp_v6.c
 * @brief The deployed model: a 51 -> 32 -> 1 ReLU MLP.
 *
 * Selected by pick_champion.py as the winner of the bebop/membo alarm criterion
 * with zero two-window false alarms, at an operating point of +7.25. That
 * threshold was later raised to 15.0 after the model was measured on hardware
 * for the first time - see DETECT_DEFAULT_THR_MILLI in detect_service.h for the
 * measurement and for what it does not prove.
 *
 * The decision is a raw logit, not a probability. A threshold that looks
 * sensible for a probability (0.5, say) is nowhere near this model's operating
 * range, which is why the threshold belongs to the model rather than to the
 * detector.
 */
#include "classifier.h"

#include "arm_math.h"
#include "mlp_model_data_v6.h"

_Static_assert(MLP_NUM_INPUTS + 1u == DET_FEATURE_COUNT,
               "mlp_v6 expects a different feature count than the aggregate produces");

/* Feature 0 is the mean of MFCC coefficient 0, which tracks absolute level.
   This family is trained to be gain-invariant, so it starts at 1. */
#define MLP_V6_OFFSET 1u

static float mlp_v6_decide(const void *ctx, const float *features)
{
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
