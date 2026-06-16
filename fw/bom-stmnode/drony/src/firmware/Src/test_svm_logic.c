/**
 * @file test_svm_logic.c
 * @brief Independent test for SVM C implementation logic using host compiler.
 */

#include <stdio.h>
#include "svm_classifier.h"
#include "svm_test_vectors.h"

int main(void) {
    printf("--- SVM C-Logic Validation Test ---\n");
    printf("Testing on %d real vectors from Parquet datasets...\n\n", NUM_TEST_VECTORS);

    int passed = 0;
    for (int i = 0; i < NUM_TEST_VECTORS; i++) {
        float decision = svm_get_decision_value(svm_test_vectors[i]);
        int prediction = svm_predict(svm_test_vectors[i]);
        int expected = svm_expected_labels[i];

        const char *result_str = (prediction == expected) ? "PASS" : "FAIL";
        if (prediction == expected) passed++;

        printf("Vector %d: Expected=%d, Predicted=%d, Decision=%8.4f [%s]\n", 
               i, expected, prediction, decision, result_str);
    }

    printf("\nTest Results: %d/%d Passed\n", passed, NUM_TEST_VECTORS);
    printf("-----------------------------------\n");

    return (passed == NUM_TEST_VECTORS) ? 0 : 1;
}
