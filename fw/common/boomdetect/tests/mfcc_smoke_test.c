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
#include "bd_test.h"
#include "boomdetect_mfcc.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

BD_TEST_STATE;

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

static void scenario_mfcc_runs(void)
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

    REQUIRE(arm_mfcc_init_1024_f32(&mfcc, NUM_MEL, NUM_DCT, dct, mel_pos, mel_len,
                                   mel_coefs, win) == ARM_MATH_SUCCESS,
            "arm_mfcc_init_1024_f32 failed");

    /* Integer LCG, so this input is identical on every platform. */
    uint32_t seed = 1u;
    for (i = 0; i < FFT_LEN; i++)
    {
        seed = (seed * 1103515245u) + 12345u;
        frame[i] = (float32_t)((int32_t)((seed >> 16) & 0xFFFFu) - 32768) / 32768.0f;
    }

    boomdetect_mfcc_f32(&mfcc, frame, out, scratch);

    float noisy[NUM_DCT];
    for (i = 0; i < NUM_DCT; i++)
    {
        CHECK(isfinite(out[i]), "coefficient %u is not finite", (unsigned)i);
        noisy[i] = out[i];
    }

    /* The rms > 1e-6f guard is one of the two things this variant changes from
       upstream, so assert what the guard actually DOES rather than that nothing
       crashed. Upstream conditions by absolute maximum and skips only when that
       is exactly 0.0f; ours skips below an RMS floor. Either way a silent frame
       must come out finite AND must not come out as the previous frame's
       coefficients, which is what a guard that returned early without writing
       would produce. */
    memset(frame, 0, sizeof(frame));
    boomdetect_mfcc_f32(&mfcc, frame, out, scratch);

    bool all_same = true;
    for (i = 0; i < NUM_DCT; i++)
    {
        CHECK(isfinite(out[i]), "silent frame produced a non-finite coefficient %u",
              (unsigned)i);
        if (out[i] != noisy[i])
        {
            all_same = false;
        }
    }
    CHECK(!all_same,
          "a silent frame produced exactly the previous frame's coefficients, so the "
          "quiet-frame guard returned without writing anything");

    /* And it is idempotent: two silent frames in a row agree, which a guard
       leaking scratch state between calls would not manage. */
    float silent_first[NUM_DCT];
    memcpy(silent_first, out, sizeof(silent_first));
    memset(frame, 0, sizeof(frame));
    boomdetect_mfcc_f32(&mfcc, frame, out, scratch);
    for (i = 0; i < NUM_DCT; i++)
    {
        CHECK(out[i] == silent_first[i],
              "two identical silent frames gave different coefficient %u (%g vs %g), so "
              "the front end is carrying state between calls",
              (unsigned)i, (double)silent_first[i], (double)out[i]);
    }
}

int main(void)
{
    scenario_mfcc_runs();
    BD_TEST_REPORT("mfcc_smoke_test", 41); /* exact count from running the compiled binary */
}
