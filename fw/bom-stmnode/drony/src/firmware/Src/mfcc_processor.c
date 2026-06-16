/**
 * @file mfcc_processor.c
 * @brief Implementation of MFCC feature extraction.
 */

#include "mfcc_processor.h"
#include <stdlib.h>

/* CMSIS-DSP MFCC Instance */
static arm_mfcc_instance_f32 mfcc_inst;

/* Internal buffers required by CMSIS-DSP MFCC */
/* Note: These sizes are specific to the FFT size used. */
static float32_t scratch_buffer[FFT_SIZE * 2];
static float32_t window_func[WINDOW_SIZE];

arm_status mfcc_init(void) {
    /* Initialize the window function (e.g., Hamming window) */
    /* In a real implementation, this could be pre-calculated. */
    for (int i = 0; i < WINDOW_SIZE; i++) {
        window_func[i] = 0.54f - 0.46f * cosf(2.0f * PI * (float32_t)i / (float32_t)(WINDOW_SIZE - 1));
    }

    /* Initialize MFCC instance */
    /* The filter bank tables are usually provided by CMSIS-DSP or pre-calculated. */
    /* Here we use the standard initialization function. */
    return arm_mfcc_init_f32(
        &mfcc_inst,
        NUM_MFCC_COEFFS,
        NUM_MEL_FILTERS,
        LOW_FREQ_HZ,
        HIGH_FREQ_HZ,
        SAMPLING_RATE,
        FFT_SIZE,
        window_func,
        NULL, /* Filter bank coefficients (NULL uses defaults if available) */
        NULL, /* DCT coefficients (NULL uses defaults) */
        NULL, /* Filter bank offsets */
        NULL, /* Filter bank lengths */
        NULL  /* DCT window */
    );
}

void mfcc_process(const float32_t *p_audio_buffer, float32_t *p_mfcc_out) {
    /* Process the input buffer */
    /* arm_mfcc_f32 takes the input, scratch buffer, and produces the coefficients. */
    arm_mfcc_f32(&mfcc_inst, (float32_t *)p_audio_buffer, p_mfcc_out, scratch_buffer);
}
