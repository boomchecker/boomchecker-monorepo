/**
 * @file svm_classifier.h
 * @brief Decision-function inference for the active model header (see
 *        svm_classifier.c): a linear SVM (scaled dot product + bias) or a
 *        small MLP / MLP ensemble (scaled inputs, one ReLU layer, raw logit).
 *        Which one is compiled in is fixed at build time by the single
 *        #include in svm_classifier.c; the name is historical.
 */

#ifndef SVM_CLASSIFIER_H
#define SVM_CLASSIFIER_H

#include <stdint.h>
#include "arm_math.h"

/**
 * @brief Initializes the SVM classifier using CMSIS-DSP.
 */
void svm_classifier_init(void);

/**
 * @brief Returns the raw decision function value (for debugging).
 * @param p_features Pointer to the input feature vector.
 * @return float Decision value.
 */
float svm_get_decision_value(const float *p_features);

#endif /* SVM_CLASSIFIER_H */
