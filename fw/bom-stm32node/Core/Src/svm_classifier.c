#include "svm_classifier.h"
/* Active model - exactly one include. Linear SVMs: svm_model_data.h (v1),
   _v2.h, _v3.h, _v2nm.h (mean-c0 dropped -> level-invariant). MLPs (decision
   is the raw logit, so the neutral detect threshold is 0 instead of 0.5):
   mlp_model_data_v4.h (26-feature layout), _v5.h (52-feature layout with
   dmean/cmax, best quiet-drone sensitivity), _v5e.h (52-feature seed
   ensemble - field candidate, best on real flyovers), _v6.h (52-feature,
   pick_champion.py winner for the bebop/membo alarm criterion; operating
   threshold +7.25 = thr_milli 7250). The MLP_ENSEMBLE / MLP_HIDDEN macros
   select the matching forward pass; MLP_NUM_INPUTS ties the header to the
   det_aggregate feature layout. */
#include "mlp_model_data_v6.h"

void svm_classifier_init(void) {
    // All model families are static tables - nothing to initialize
}

#if defined(MLP_ENSEMBLE)

float svm_get_decision_value(const float *p_features) {
    /* Mean logit of MLP_ENSEMBLE independently trained members; each member
       has its own StandardScaler. features[0] (mean-c0) is excluded. */
    float32_t x[MLP_NUM_INPUTS];
    float32_t h[MLP_HIDDEN];
    float32_t sum = 0.0f;

    for (int e = 0; e < MLP_ENSEMBLE; e++) {
        for (int i = 0; i < MLP_NUM_INPUTS; i++) {
            x[i] = (p_features[i + 1] - mlp_scaler_mean[e][i])
                   * mlp_scaler_inv_std[e][i];
        }
        for (int j = 0; j < MLP_HIDDEN; j++) {
            float32_t acc = 0.0f;
            arm_dot_prod_f32(x, mlp_w1[e][j], MLP_NUM_INPUTS, &acc);
            acc += mlp_b1[e][j];
            h[j] = (acc > 0.0f) ? acc : 0.0f;
        }
        float32_t out = 0.0f;
        arm_dot_prod_f32(h, mlp_w2[e], MLP_HIDDEN, &out);
        sum += out + mlp_b2[e];
    }
    return sum / (float32_t)MLP_ENSEMBLE;
}

#elif defined(MLP_HIDDEN)

float svm_get_decision_value(const float *p_features) {
    /* features[0] (mean-c0) carries absolute level and is excluded; the
       remaining 25 features are gain-invariant. x = scaled inputs,
       h = ReLU hidden layer, output = logit. */
    float32_t x[MLP_NUM_INPUTS];
    float32_t h[MLP_HIDDEN];

    for (int i = 0; i < MLP_NUM_INPUTS; i++) {
        x[i] = (p_features[i + 1] - mlp_scaler_mean[i]) * mlp_scaler_inv_std[i];
    }
    for (int j = 0; j < MLP_HIDDEN; j++) {
        float32_t acc = 0.0f;
        arm_dot_prod_f32(x, mlp_w1[j], MLP_NUM_INPUTS, &acc);
        acc += mlp_b1[j];
        h[j] = (acc > 0.0f) ? acc : 0.0f;
    }
    float32_t out = 0.0f;
    arm_dot_prod_f32(h, mlp_w2, MLP_HIDDEN, &out);
    return out + MLP_B2;
}

#else /* linear SVM */

float svm_get_decision_value(const float *p_features) {
    float32_t scaled_features[SVM_NUM_FEATURES];

    // Apply StandardScaler scaling: (x - mean) * (1 / std)
    for (int i = 0; i < SVM_NUM_FEATURES; i++) {
        scaled_features[i] = (p_features[i] - svm_scaler_mean[i]) * svm_scaler_inv_std[i];
    }

    // Calculate dot product: w * scaled_x
    float32_t dot_prod = 0.0f;
    arm_dot_prod_f32(scaled_features, svm_weights, SVM_NUM_FEATURES, &dot_prod);

    return dot_prod + SVM_BIAS;
}

#endif

int svm_predict(const float *p_features) {
    float32_t decision = svm_get_decision_value(p_features);
    return (decision >= 0.5f) ? 1 : 0;
}
