/**
 * @file svm_classifier.c
 * @brief Implementation of SVM inference with RBF kernel.
 */

#include "svm_classifier.h"
#include "svm_model_data.h"
#include <math.h>

/**
 * @brief Calculates the RBF (Gaussian) kernel between two vectors.
 * K(x, y) = exp(-gamma * ||x - y||^2)
 */
static float rbf_kernel(const float *x, const float *y) {
    float sum_sq_diff = 0.0f;
    for (int i = 0; i < SVM_NUM_FEATURES; i++) {
        float diff = x[i] - y[i];
        sum_sq_diff += diff * diff;
    }
    return expf(-SVM_GAMMA * sum_sq_diff);
}

float svm_get_decision_value(const float *p_features) {
    float decision = SVM_INTERCEPT;

    for (int i = 0; i < SVM_NUM_SUPPORT_VECTORS; i++) {
        float kernel_val = rbf_kernel(p_features, svm_support_vectors[i]);
        decision += svm_dual_coefficients[i] * kernel_val;
    }

    return decision;
}

int svm_predict(const float *p_features) {
    float decision = svm_get_decision_value(p_features);
    
    // For binary classification in scikit-learn SVM:
    // decision > 0 -> class 1
    // decision < 0 -> class 0
    return (decision > 0.0f) ? 1 : 0;
}
