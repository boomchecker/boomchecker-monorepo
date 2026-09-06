/**
 * @file model_svm_v3.c
 * @brief Level-robust linear SVM over the 26-value [mean, std] prefix.
 *
 * Not the deployed model. It is here because it is the only other family in
 * tree and it consumes a genuinely different slice of the feature vector - a
 * different offset AND a different count - which is the one thing that makes
 * the registry's dispatch worth testing rather than asserting. A second entry
 * that read the same features the same way would exercise a name lookup and
 * nothing else.
 */
#include "classifier.h"

#include "arm_math.h"
#include "svm_model_data_v3.h"

_Static_assert(SVM_NUM_FEATURES <= DET_FEATURE_COUNT,
               "svm_v3 reads more features than the aggregate produces");

static float svm_v3_decide(const void *ctx, const float *features)
{
    float32_t scaled[SVM_NUM_FEATURES];
    float32_t dot = 0.0f;

    (void)ctx;

    for (int i = 0; i < SVM_NUM_FEATURES; i++)
    {
        scaled[i] = (features[i] - svm_scaler_mean[i]) * svm_scaler_inv_std[i];
    }
    arm_dot_prod_f32(scaled, svm_weights, SVM_NUM_FEATURES, &dot);
    return dot + SVM_BIAS;
}

const classifier_t classifier_svm_v3 = {
    .name              = "svm_v3",
    .layout_id         = BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX,
    .n_features        = SVM_NUM_FEATURES,
    .feature_offset    = 0u,
    /* 500, not 15000: this family's decision lives around +-3, so the MLP's
       logit threshold would be a detector that never fires. */
    .default_thr_milli = 500,
    .decide            = svm_v3_decide,
    .ctx               = NULL,
};
