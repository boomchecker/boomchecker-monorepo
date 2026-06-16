/**
 * @file main_dsp_test.c
 * @brief Complete pipeline test: Audio -> MFCC -> Feature Aggregation -> SVM.
 */

#include <stdio.h>
#include <math.h>
#include "mfcc_processor.h"
#include "svm_classifier.h"
#include "mfcc_parity_data.h"

static float32_t mfcc_buffer[NUM_MFCC_COEFFS];
static float32_t feature_vector[SVM_NUM_FEATURES]; // 26 features: 13 means + 13 stds

void aggregate_features(const float32_t *p_mfcc_frames, int num_frames, float32_t *p_out) {
    // p_out will have 26 elements: [mean0..12, std0..12]
    
    for (int c = 0; c < NUM_MFCC_COEFFS; c++) {
        float32_t sum = 0.0f;
        for (int f = 0; f < num_frames; f++) {
            sum += p_mfcc_frames[f * NUM_MFCC_COEFFS + c];
        }
        float32_t mean = sum / (float32_t)num_frames;
        p_out[c] = mean;

        float32_t sum_sq_diff = 0.0f;
        for (int f = 0; f < num_frames; f++) {
            float32_t diff = p_mfcc_frames[f * NUM_MFCC_COEFFS + c] - mean;
            sum_sq_diff += diff * diff;
        }
        p_out[c + NUM_MFCC_COEFFS] = sqrtf(sum_sq_diff / (float32_t)num_frames);
    }
}

int main(void) {
    printf("--- Drone Detection Full Pipeline Test ---\n");

    /* 1. Initialize MFCC */
    if (mfcc_init() != ARM_MATH_SUCCESS) {
        printf("Error: MFCC Initialization failed!\n");
        return -1;
    }
    printf("MFCC Initialized.\n");

    /* 2. Process all frames of the test signal */
    // We'll store all MFCC frames to aggregate them later
    float32_t all_mfccs[MFCC_TEST_NUM_FRAMES * NUM_MFCC_COEFFS];
    int processed_frames = 0;

    for (int f = 0; f < MFCC_TEST_NUM_FRAMES; f++) {
        int start_idx = f * 512;
        if (start_idx + WINDOW_SIZE > MFCC_TEST_SIGNAL_LEN) break;

        mfcc_process(&mfcc_test_signal[start_idx], &all_mfccs[f * NUM_MFCC_COEFFS]);
        processed_frames++;
    }
    printf("Processed %d audio frames.\n", processed_frames);

    /* 3. Aggregate features (Mean + Std) */
    aggregate_features(all_mfccs, processed_frames, feature_vector);
    printf("Features aggregated into %d-dim vector.\n", SVM_NUM_FEATURES);

    /* 4. Run SVM Inference */
    float decision = svm_get_decision_value(feature_vector);
    int prediction = svm_predict(feature_vector);

    printf("\n--- SVM Results ---\n");
    printf("Decision Value: %f\n", decision);
    printf("Prediction: %s (Class %d)\n", (prediction == 1) ? "DRONE" : "NOISE", prediction);
    printf("-------------------\n");

    printf("\nPipeline Test Complete.\n");
    return 0;
}
