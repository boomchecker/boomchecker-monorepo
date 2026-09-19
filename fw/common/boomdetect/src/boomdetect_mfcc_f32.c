/* ----------------------------------------------------------------------
 * Derived from the CMSIS DSP Library, Source/TransformFunctions/arm_mfcc_f32.c
 * (release v1.15.0). Copyright (C) 2010-2021 ARM Limited or its affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * -------------------------------------------------------------------- */

/**
 * @file boomdetect_mfcc_f32.c
 * @brief MFCC with RMS pre-FFT conditioning instead of upstream's absolute
 *        maximum.
 *
 * WHY THIS FILE EXISTS AT ALL
 *
 * The vendored CMSIS-DSP this firmware shipped until now was v1.15.0 with one
 * hand-edit nobody recorded: arm_mfcc_f32() conditioned its input for the FFT
 * by dividing through the RMS rather than the absolute maximum, and guarded on
 * `rms > 1e-6f` rather than `max != 0.0f`. Scaling in and back out is linear
 * and cancels in exact arithmetic, so this is not a change of definition - but
 * it does change float32 rounding, and it changes behaviour for frames quieter
 * than the guard, which upstream still normalises and this version leaves
 * alone.
 *
 * That matters because the deployed model (mlp_model_data_v6.h) was selected
 * against MFCC values produced by this code path. Fetching pristine upstream
 * would silently move every coefficient and invalidate the operating point the
 * model was chosen at.
 *
 * Keeping the deviation as our own translation unit, rather than patching the
 * fetched tree, means: upstream is fetched untouched (no PATCH_COMMAND to
 * re-run and fail on the second configure), the deviation is a reviewable
 * source file instead of a diff applied by the build, and the licence
 * attribution sits where a reader will find it.
 *
 * This function calls only public CMSIS-DSP API, which is what makes living
 * outside the library possible.
 */

#include "boomdetect_mfcc.h"

#include "dsp/basic_math_functions.h"
#include "dsp/complex_math_functions.h"
#include "dsp/fast_math_functions.h"
#include "dsp/matrix_functions.h"
#include "dsp/statistics_functions.h"
#include "dsp/transform_functions.h"

void boomdetect_mfcc_f32(const arm_mfcc_instance_f32 *S,
                         float32_t *pSrc,
                         float32_t *pDst,
                         float32_t *pTmp)
{
    const float32_t *coefs = S->filterCoefs;
    arm_matrix_instance_f32 pDctMat;
    float32_t rmsValue;
    float32_t result;
    uint32_t i;

    /* Condition the input. Upstream divides by the absolute maximum; this
       divides by the RMS. Both are undone again after the magnitude spectrum
       is taken, so the pair cancels; the choice only shows in rounding and in
       which frames are considered too quiet to touch. */
    arm_rms_f32(pSrc, S->fftLen, &rmsValue);
    if (rmsValue > 1e-6f)
    {
        arm_scale_f32(pSrc, 1.0f / rmsValue, pSrc, S->fftLen);
    }

    arm_mult_f32(pSrc, S->windowCoefs, pSrc, S->fftLen);

    /* RFFT path only. Upstream also carries an ARM_MFCC_CFFT_BASED variant for
       boards whose accelerator only does CFFT; this firmware has no such
       accelerator, so that branch is dropped rather than carried untested. */
    arm_rfft_fast_f32(&(S->rfft), pSrc, pTmp, 0);
    pTmp[S->fftLen] = pTmp[1];
    pTmp[S->fftLen + 1] = 0.0f;
    pTmp[1] = 0.0f;

    /* NOTE on pTmp sizing: this asks for fftLen magnitudes, i.e. it reads
       2*fftLen floats, while the RFFT above only wrote fftLen+2 valid ones.
       Bins above fftLen/2 are therefore computed from whatever else is in the
       buffer - harmless here because the mel filter bank never reaches them,
       but it means pTmp must be 2*fftLen long, NOT the "FFT Length + 2" the
       upstream doc comment suggests. A caller who believes the doc gets an
       out-of-bounds read that ASan will (correctly) fail the build over. */
    arm_cmplx_mag_f32(pTmp, pSrc, S->fftLen);
    if (rmsValue > 1e-6f)
    {
        arm_scale_f32(pSrc, rmsValue, pSrc, S->fftLen);
    }

    for (i = 0; i < S->nbMelFilters; i++)
    {
        arm_dot_prod_f32(pSrc + S->filterPos[i], coefs, S->filterLengths[i], &result);
        coefs += S->filterLengths[i];
        pTmp[i] = result;
    }

    arm_offset_f32(pTmp, 1.0e-6f, pTmp, S->nbMelFilters);
    arm_vlog_f32(pTmp, pTmp, S->nbMelFilters);

    pDctMat.numRows = S->nbDctOutputs;
    pDctMat.numCols = S->nbMelFilters;
    pDctMat.pData = (float32_t *)S->dctCoefs;

    arm_mat_vec_mult_f32(&pDctMat, pTmp, pDst);
}
