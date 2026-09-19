/**
 * @file main_dsp_test.c
 * @brief Complete pipeline test: Audio -> MFCC -> Feature Aggregation -> SVM.
 */

#include <stdio.h>
#include <math.h>
#include "mfcc_processor.h"
#include "svm_classifier.h"
#include "svm_model_data.h"
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
    setbuf(stdout, NULL);
    printf("--- Drone Detection Full Pipeline Test ---\n");

    /* 1. Squelch Gate (calculate signal energy) */
    float32_t signal_rms = 0.0f;
    arm_rms_f32(mfcc_test_signal, MFCC_TEST_SIGNAL_LEN, &signal_rms);
    printf("Signal RMS: %f (Squelch threshold: 0.0100)\n", signal_rms);
    
    if (signal_rms < 0.010f) {
        printf("\n--- Squelch Gate: TRIGGERED ---\n");
        printf("Signal energy too low. Skipped pipeline.\n");
        printf("Prediction: NOISE (Class 0)\n");
        printf("-------------------------------\n");
        return 0;
    }

    /* 2. Initialize MFCC and SVM */
    if (mfcc_init() != ARM_MATH_SUCCESS) {
        printf("Error: MFCC Initialization failed!\n");
        return -1;
    }
    printf("MFCC Initialized.\n");
    
    svm_classifier_init();
    printf("SVM Initialized.\n");

    /* 2. Process all frames of the test signal */
    // We'll store all MFCC frames to aggregate them later
    float32_t all_mfccs[MFCC_TEST_NUM_FRAMES * NUM_MFCC_COEFFS];
    float32_t frame_buffer[WINDOW_SIZE];
    int processed_frames = 0;

    for (int f = 0; f < MFCC_TEST_NUM_FRAMES; f++) {
        int start_idx = f * 512;
        if (start_idx + WINDOW_SIZE > MFCC_TEST_SIGNAL_LEN) break;

        // Copy to a mutable buffer because arm_mfcc_f32 modifies the input buffer in-place
        for (int i = 0; i < WINDOW_SIZE; i++) {
            frame_buffer[i] = mfcc_test_signal[start_idx + i];
        }

        mfcc_process(frame_buffer, &all_mfccs[f * NUM_MFCC_COEFFS]);
        processed_frames++;
    }
    printf("Processed %d audio frames.\n", processed_frames);

    /* 2.5. Validate MFCC Parity against Python reference */
    float32_t max_diff = 0.0f;
    float32_t sum_diff = 0.0f;
    int num_elements = processed_frames * NUM_MFCC_COEFFS;
    
    for (int f = 0; f < processed_frames; f++) {
        for (int c = 0; c < NUM_MFCC_COEFFS; c++) {
            float32_t diff = fabsf(all_mfccs[f * NUM_MFCC_COEFFS + c] - mfcc_reference_output[f][c]);
            if (diff > max_diff) max_diff = diff;
            sum_diff += diff;
        }
    }
    float32_t avg_diff = sum_diff / (float32_t)num_elements;
    printf("\n--- MFCC Parity Check ---\n");
    printf("Max difference: %e\n", max_diff);
    printf("Average difference: %e\n", avg_diff);
    if (avg_diff < 0.05f) {
        printf("MFCC Parity Check: PASS\n");
    } else {
        printf("MFCC Parity Check: FAIL\n");
    }
    printf("-------------------------\n");

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
