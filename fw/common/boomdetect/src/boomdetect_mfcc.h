/**
 * @file boomdetect_mfcc.h
 * @brief MFCC front end, deviating from stock CMSIS-DSP in one documented way.
 */
#ifndef BOOMDETECT_MFCC_H
#define BOOMDETECT_MFCC_H

#include "arm_math.h"
#include "dsp/transform_functions.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Drop-in replacement for arm_mfcc_f32() that conditions the FFT input
 *        by RMS rather than by absolute maximum.
 *
 * Same signature, same instance type, same output layout as upstream. See
 * boomdetect_mfcc_f32.c for why the deviation exists and why it must not be
 * "fixed" back to upstream: the deployed model was selected against the values
 * this version produces.
 *
 * @param S    initialised MFCC instance (arm_mfcc_init_*_f32)
 * @param pSrc input frame of S->fftLen samples. DESTROYED by this call.
 * @param pDst output, S->nbDctOutputs coefficients
 * @param pTmp scratch, and it must be 2*S->fftLen floats - not the
 *             "FFT length + 2" upstream's documentation states. See the note
 *             in the implementation.
 */
void boomdetect_mfcc_f32(const arm_mfcc_instance_f32 *S,
                         float32_t *pSrc,
                         float32_t *pDst,
                         float32_t *pTmp);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_MFCC_H */
