/**
 * @file mfcc_processor.h
 * @brief Header for MFCC feature extraction using CMSIS-DSP.
 */

#ifndef MFCC_PROCESSOR_H
#define MFCC_PROCESSOR_H

#include "arm_math.h"
#include "dsp_config.h"

/**
 * @brief Initializes the MFCC processing instance.
 * @return arm_status Initialization status.
 */
arm_status mfcc_init(void);

/**
 * @brief Processes a window of audio samples to extract MFCC features.
 * @param p_audio_buffer input samples (float32_t[WINDOW_SIZE]). DESTROYED: the
 *        transform windows and overwrites it in place. Pass a scratch copy.
 * @param p_mfcc_out Pointer to the output feature vector (float32_t[NUM_MFCC_COEFFS]).
 */
void mfcc_process(float32_t *p_audio_buffer, float32_t *p_mfcc_out);

#endif /* MFCC_PROCESSOR_H */
