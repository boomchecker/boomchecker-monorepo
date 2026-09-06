/**
 * @file mfcc_processor.c
 * @brief MFCC feature extraction over the generated mel/DCT tables.
 */

#include "mfcc_processor.h"

#include "boomdetect_mfcc.h"
#include "mfcc_tables.h"

static arm_mfcc_instance_f32 mfcc_inst;

/* 2*FFT_SIZE, and it has to be: the magnitude step inside the MFCC asks for
   fftLen magnitudes and so reads twice that many floats, while upstream's doc
   comment claims "FFT length + 2" is enough. See boomdetect_mfcc_f32.c. */
static float32_t scratch_buffer[FFT_SIZE * 2];

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
