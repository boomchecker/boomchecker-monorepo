/**
 * @file svm_classifier.c
 * @brief Implementation of SVM inference with RBF kernel.
 */

#include "svm_classifier.h"
#include "svm_model_data.h"

// CMSIS-DSP SVM Instance
static arm_svm_rbf_instance_f32 svm_inst;

// Scikit-learn SVM classes mapped for CMSIS-DSP (e.g., 0 and 1)
static const int32_t svm_classes[2] = {0, 1};

void svm_classifier_init(void) {
    arm_svm_rbf_init_f32(&svm_inst,
        SVM_NUM_SUPPORT_VECTORS,
        SVM_NUM_FEATURES,
        SVM_INTERCEPT,
        svm_dual_coefficients,
        (const float32_t *)svm_support_vectors,
        svm_classes,
        SVM_GAMMA
    );
}

int svm_predict(const float *p_features) {
    int32_t result;
    arm_svm_rbf_predict_f32(&svm_inst, p_features, &result);
    return (int)result;
}

// Custom implementation to get the raw decision value for debugging
// using CMSIS-DSP vector math.
float svm_get_decision_value(const float *p_features) {
    float32_t decision = SVM_INTERCEPT;
    float32_t sum_sq_diff;
    float32_t diff_buffer[SVM_NUM_FEATURES];

    for (int i = 0; i < SVM_NUM_SUPPORT_VECTORS; i++) {
        // Calculate (x - y)
        arm_sub_f32(p_features, (const float32_t *)svm_support_vectors[i], diff_buffer, SVM_NUM_FEATURES);
        // Calculate ||x - y||^2
        arm_dot_prod_f32(diff_buffer, diff_buffer, SVM_NUM_FEATURES, &sum_sq_diff);
        
        float32_t kernel_val = expf(-SVM_GAMMA * sum_sq_diff);
        decision += svm_dual_coefficients[i] * kernel_val;
    }

    return decision;
}
