/**
 * @file mfcc_processor.c
 * @brief Implementation of MFCC feature extraction.
 */

#include "mfcc_processor.h"
#include "mfcc_tables.h"
#include <stdlib.h>

/* CMSIS-DSP MFCC Instance */
static arm_mfcc_instance_f32 mfcc_inst;

/* Internal buffers required by CMSIS-DSP MFCC */
/* Note: These sizes are specific to the FFT size used. */
static float32_t scratch_buffer[FFT_SIZE * 2];

arm_status mfcc_init(void) {
    /* Initialize MFCC instance using generated static tables */
    return arm_mfcc_init_1024_f32(
        &mfcc_inst,
        MFCC_NUM_MEL_FILTERS,
        MFCC_DCT_ROWS,
        (const float32_t *)mfcc_dct_coefs,
        mfcc_filter_pos,
        mfcc_filter_lengths,
        mfcc_filter_coefs,
        mfcc_window_coefs
    );
}

void mfcc_process(const float32_t *p_audio_buffer, float32_t *p_mfcc_out) {
    /* Process the input buffer */
    /* arm_mfcc_f32 takes the input, scratch buffer, and produces the coefficients. */
    arm_mfcc_f32(&mfcc_inst, (float32_t *)p_audio_buffer, p_mfcc_out, scratch_buffer);
}
