/**
 * @file mfcc_processor.h
 * @brief Header for MFCC feature extraction using CMSIS-DSP.
 */
#ifndef BOOMDETECT_MFCC_PROCESSOR_H
#define BOOMDETECT_MFCC_PROCESSOR_H

#include "arm_math.h"
#include "dsp_config.h"

/**
 * @brief Initializes the MFCC processing instance.
 * @return arm_status Initialization status.
 */
arm_status mfcc_init(void);

/**
 * @brief Processes a window of audio samples to extract MFCC features.
 * @param p_audio_buffer input samples (float32_t[BOOMDETECT_WINDOW_SIZE]). DESTROYED: the
 *        transform windows and overwrites it in place. On return it holds the
 *        MAGNITUDE SPECTRUM of the windowed frame - bins 0..BOOMDETECT_FFT_SIZE/2 are
 *        meaningful, the rest is scratch - which is what the per-frame spectral
 *        scalars (frame_scalars.h) are computed from. Pass a scratch copy.
 * @param p_mfcc_out Pointer to the output feature vector (float32_t[BOOMDETECT_MFCC_COEFFS]).
 */
void mfcc_process(float32_t *p_audio_buffer, float32_t *p_mfcc_out);

/**
 * @brief The log-mel energies the last mfcc_process() applied the DCT to.
 *
 * BOOMDETECT_MEL_FILTERS values, ln(mel + 1e-6), valid until the next call.
 * They sit in the transform's scratch buffer already, so exposing them costs
 * nothing; recomputing them would cost a second pass over the filter bank.
 */
const float32_t *mfcc_last_logmel(void);

#endif /* BOOMDETECT_MFCC_PROCESSOR_H */
