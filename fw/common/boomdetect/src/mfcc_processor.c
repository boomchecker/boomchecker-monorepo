/**
 * @file mfcc_processor.c
 * @brief MFCC feature extraction over the generated mel/DCT tables.
 */

#include "mfcc_processor.h"

#include "boomdetect_mfcc.h"
#include "mfcc_tables.h"

/* Two independent sets of dimensions meet here: dsp_config.h's (which size the
   buffers in boomdetect_t) and mfcc_tables.h's (which the generator wrote). They
   agree today and nothing made them. A regenerated table with a different
   coefficient count would have mfcc_process() write MFCC_DCT_ROWS floats into a
   slot strided by BOOMDETECT_MFCC_COEFFS - an overrun that stays inside boomdetect_t,
   so ASan never sees it and only the decisions go quietly wrong. */
_Static_assert(MFCC_DCT_ROWS == BOOMDETECT_MFCC_COEFFS,
               "mfcc_tables.h was generated for a different coefficient count");
_Static_assert(MFCC_DCT_COLS == BOOMDETECT_MEL_FILTERS,
               "mfcc_tables.h was generated for a different mel filter count");
_Static_assert(MFCC_NUM_MEL_FILTERS == BOOMDETECT_MEL_FILTERS,
               "mfcc_tables.h was generated for a different mel filter count");
_Static_assert(MFCC_WINDOW_LEN == BOOMDETECT_WINDOW_SIZE,
               "mfcc_tables.h was generated for a different window length");
/* And the CMSIS entry point below hardcodes fftLen, so the buffers sized from
   these macros must match it rather than merely being large enough. */
_Static_assert(BOOMDETECT_WINDOW_SIZE == 1024 && BOOMDETECT_FFT_SIZE == 1024,
               "arm_mfcc_init_1024_f32 fixes fftLen at 1024; frame[] and "
               "scratch_buffer[] are sized from BOOMDETECT_WINDOW_SIZE/BOOMDETECT_FFT_SIZE");

static arm_mfcc_instance_f32 mfcc_inst;

/* 2*BOOMDETECT_FFT_SIZE, and it has to be: the magnitude step inside the MFCC asks for
   fftLen magnitudes and so reads twice that many floats, while upstream's doc
   comment claims "FFT length + 2" is enough. See boomdetect_mfcc_f32.c. */
static float32_t scratch_buffer[BOOMDETECT_FFT_SIZE * 2];

arm_status mfcc_init(void)
{
    return arm_mfcc_init_1024_f32(&mfcc_inst,
                                  MFCC_NUM_MEL_FILTERS,
                                  MFCC_DCT_ROWS,
                                  (const float32_t *)mfcc_dct_coefs,
                                  mfcc_filter_pos,
                                  mfcc_filter_lengths,
                                  mfcc_filter_coefs,
                                  mfcc_window_coefs);
}

void mfcc_process(float32_t *p_audio_buffer, float32_t *p_mfcc_out)
{
    /* Not const, and the caller has to know it: the MFCC windows and transforms
       the input in place. The old declaration said const and cast it away one
       line later, which is the kind of thing that reads as a formality until
       someone passes a buffer they still needed. */
    boomdetect_mfcc_f32(&mfcc_inst, p_audio_buffer, p_mfcc_out, scratch_buffer);
}
