/**
 * @file mfcc_smoke_test.c
 * @brief Proves the MFCC front end builds, links and runs on the host.
 *
 * Deliberately not a parity or correctness test - those need the generated mel
 * and DCT tables, which arrive with the detector itself. This one answers the
 * question that has to be answered before any code is moved into this package:
 * does CMSIS-DSP configure, compile and link for an x86 host at all, and does
 * our MFCC variant run without tripping a sanitizer.
 */
#include "boomdetect_mfcc.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define FFT_LEN     1024u
#define NUM_MEL     20u
#define NUM_DCT     13u

/* Flat, deliberately trivial tables: one mel filter per bin group with unit
   weight, identity-ish DCT. Numerically meaningless, which is the point - the
   real tables are generated and belong with the detector. */
static float32_t win[FFT_LEN];
static float32_t mel_coefs[NUM_MEL];
static uint32_t  mel_pos[NUM_MEL];
static uint32_t  mel_len[NUM_MEL];
static float32_t dct[NUM_DCT * NUM_MEL];

/* 2*fftLen, NOT fftLen+2: arm_cmplx_mag_f32() inside the MFCC asks for fftLen
   magnitudes and therefore reads twice that many floats. See the note in
   boomdetect_mfcc_f32.c - undersizing this is the exact bug the sanitizer
   build exists to catch. */
static float32_t scratch[2u * FFT_LEN];
static float32_t frame[FFT_LEN];
static float32_t out[NUM_DCT];

int main(void)
{
    arm_mfcc_instance_f32 mfcc;
    uint32_t i;

    for (i = 0; i < FFT_LEN; i++)
    {
        win[i] = 1.0f;
    }
    for (i = 0; i < NUM_MEL; i++)
    {
        mel_coefs[i] = 1.0f;
        mel_pos[i] = i;
        mel_len[i] = 1u;
    }
    for (i = 0; i < NUM_DCT * NUM_MEL; i++)
    {
        dct[i] = (i % (NUM_MEL + 1u) == 0u) ? 1.0f : 0.0f;
    }

    if (arm_mfcc_init_1024_f32(&mfcc, NUM_MEL, NUM_DCT, dct, mel_pos, mel_len,
                               mel_coefs, win) != ARM_MATH_SUCCESS)
    {
        printf("FAIL: arm_mfcc_init_1024_f32\n");
        return 1;
    }

    /* Integer LCG, so this input is identical on every platform. */
    uint32_t seed = 1u;
    for (i = 0; i < FFT_LEN; i++)
    {
        seed = (seed * 1103515245u) + 12345u;
        frame[i] = (float32_t)((int32_t)((seed >> 16) & 0xFFFFu) - 32768) / 32768.0f;
    }

    boomdetect_mfcc_f32(&mfcc, frame, out, scratch);

    for (i = 0; i < NUM_DCT; i++)
    {
        if (!isfinite(out[i]))
        {
            printf("FAIL: coefficient %u is not finite\n", (unsigned)i);
            return 1;
        }
    }

    /* A silent frame must survive the rms > 1e-6f guard rather than dividing
       by (almost) zero - that guard is one of the two things this variant
       changes from upstream, so exercise it. */
    memset(frame, 0, sizeof(frame));
    boomdetect_mfcc_f32(&mfcc, frame, out, scratch);
    for (i = 0; i < NUM_DCT; i++)
    {
        if (!isfinite(out[i]))
        {
            printf("FAIL: silent frame produced a non-finite coefficient %u\n", (unsigned)i);
            return 1;
        }
    }

    printf("PASS: CMSIS-DSP builds for the host and the MFCC front end runs\n");
    return 0;
}
