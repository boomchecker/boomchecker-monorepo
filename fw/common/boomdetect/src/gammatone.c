/**
 * @file gammatone.c
 * @brief The gammatone band spectrogram; see gammatone.h.
 *
 * Float32 where the Python runs float64: the band sums agree to about 1e-6
 * relative, a few 1e-6 after the log, which tests/vectors/
 * extractor_gs_expected.h holds the rows to.
 */
#include "gammatone.h"

#include "arm_math.h"
#include "gammatone_table.h"

#include <math.h>

#define GS_FFT    BOOMDETECT_GS_TABLE_FFT
#define GS_BINS   (GS_FFT / 2u + 1u)
#define GS_TWO_PI 6.283185307179586

_Static_assert(BOOMDETECT_GS_TABLE_BANDS == BOOMDETECT_GS_BANDS,
               "gammatone_table.h was generated for a different band count");
_Static_assert(BOOMDETECT_GS_TABLE_FFT == BOOMDETECT_WINDOW_SIZE,
               "the gammatone FFT is taken over one frame");

/* Static: the superloop's stack is small, and one detector runs a frame at a
   time - the same reasoning as the MFCC's scratch in mfcc_processor.c. */
static arm_rfft_fast_instance_f32 s_rfft;
static bool                       s_ready = false;
static float                      s_hann[GS_FFT];
static float                      s_in[GS_FFT];
static float                      s_out[GS_FFT];
static float                      s_pow[GS_BINS];

bool boomdetect_gs_prepare(void)
{
    if (!s_ready)
    {
        if (arm_rfft_fast_init_f32(&s_rfft, (uint16_t)GS_FFT) != ARM_MATH_SUCCESS)
        {
            return false;
        }
        /* spectro.hann_periodic(): 0.5 - 0.5 cos(2 pi n / N), the periodic window
           (N in the denominator, not N - 1). In double, so the table lands on
           numpy's values to float32 rounding. */
        for (uint32_t n = 0u; n < GS_FFT; n++)
        {
            s_hann[n] = (float)(0.5 - 0.5 * cos(GS_TWO_PI * (double)n / (double)GS_FFT));
        }
        s_ready = true;
    }
    return true;
}

void boomdetect_gs_row(const float *frame, float *row)
{
    for (uint32_t n = 0u; n < GS_FFT; n++)
    {
        s_in[n] = frame[n] * s_hann[n];
    }
    arm_rfft_fast_f32(&s_rfft, s_in, s_out, 0u);
    /* CMSIS packs the real Nyquist value into the imaginary slot of DC. */
    s_pow[0]           = s_out[0] * s_out[0];
    s_pow[GS_BINS - 1u] = s_out[1] * s_out[1];
    for (uint32_t k = 1u; k < GS_BINS - 1u; k++)
    {
        const float re = s_out[2u * k];
        const float im = s_out[2u * k + 1u];
        s_pow[k]       = re * re + im * im;
    }
    /* Each band's weights are one contiguous run of bins; dividing by the
       window's energy once per band is the per-bin division of the Python. */
    const float *w = boomdetect_gs_weights;
    for (uint32_t b = 0u; b < BOOMDETECT_GS_BANDS; b++)
    {
        const float   *p   = &s_pow[boomdetect_gs_start[b]];
        const uint32_t len = boomdetect_gs_len[b];
        float          acc = 0.0f;
        for (uint32_t j = 0u; j < len; j++)
        {
            acc += w[j] * p[j];
        }
        w += len;
        row[b] = logf(acc / BOOMDETECT_GS_POWER_NORM + BOOMDETECT_GS_LOG_OFFSET);
    }
}

void boomdetect_gs_reset(boomdetect_t *d)
{
    d->gs_head = 0u;
    d->gs_fill = 0u;
}

void boomdetect_gs_push(boomdetect_t *d, const float *frame)
{
    uint32_t slot;
    if (d->gs_fill < BOOMDETECT_GS_RING_ROWS)
    {
        slot = (d->gs_head + d->gs_fill) % BOOMDETECT_GS_RING_ROWS;
        d->gs_fill++;
    }
    else
    {
        slot       = d->gs_head;
        d->gs_head = (d->gs_head + 1u) % BOOMDETECT_GS_RING_ROWS;
    }
    boomdetect_gs_row(frame, &d->gs_ring[slot * BOOMDETECT_GS_BANDS]);
}
