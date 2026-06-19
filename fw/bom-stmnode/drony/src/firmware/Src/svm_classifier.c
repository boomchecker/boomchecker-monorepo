#include "svm_classifier.h"
#include "svm_model_data.h"

void svm_classifier_init(void) {
    // Linear model doesn't require dynamic initialization
}

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

int svm_predict(const float *p_features) {
    float32_t decision = svm_get_decision_value(p_features);
    return (decision >= 0.5f) ? 1 : 0;
}
