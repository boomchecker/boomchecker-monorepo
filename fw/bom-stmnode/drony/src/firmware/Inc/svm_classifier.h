/**
 * @file svm_classifier.h
 * @brief Header for SVM inference using RBF kernel.
 */

#ifndef SVM_CLASSIFIER_H
#define SVM_CLASSIFIER_H

#include <stdint.h>
#include "arm_math.h"
#include "dsp/svm_functions.h"

/**
 * @brief Initializes the SVM classifier using CMSIS-DSP.
 */
void svm_classifier_init(void);

/**
 * @brief Predicts the class based on input feature vector.
 * @param p_features Pointer to the input feature vector (float[SVM_NUM_FEATURES]).
 * @return int 1 if drone detected, 0 otherwise.
 */
int svm_predict(const float *p_features);

/**
 * @brief Returns the raw decision function value (for debugging).
 * @param p_features Pointer to the input feature vector.
 * @return float Decision value.
 */
float svm_get_decision_value(const float *p_features);

#endif /* SVM_CLASSIFIER_H */
