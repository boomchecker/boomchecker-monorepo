/**
 ******************************************************************************
 * @file    pdm_pcm.c
 * @brief   PDM -> PCM DSP (CIC5 + DC blocker + FIR). Ported from Mik_stm.
 ******************************************************************************
 */
#include "pdm_pcm.h"
#include <string.h>

/* FIR low-pass 8 kHz @ 48 kHz, q15, 101 taps (windowed sinc, Hanning, same
   design as Mik_stm/tools/pdm_capture.py). Sum = 32768 -> DC gain 1.0.
   Sum |h| = 61684 -> worst-case 32-bit accumulator 32767*61684 < INT32_MAX. */
static const int16_t fir_lp[FIR_TAPS] = {
       0,      0,      0,     -2,     -3,      0,      7,     10,      0,    -17,
     -22,      0,     32,     39,      0,    -53,    -62,      0,     81,     92,
       0,   -117,   -131,      0,    163,    181,      0,   -221,   -244,      0,
     296,    325,      0,   -394,   -434,      0,    528,    585,      0,   -727,
    -817,      0,   1059,   1229,      0,  -1762,  -2223,      0,   4499,   9024,
   10926,   9024,   4499,      0,  -2223,  -1762,      0,   1229,   1059,      0,
    -817,   -727,      0,    585,    528,      0,   -434,   -394,      0,    325,
     296,      0,   -244,   -221,      0,    181,    163,      0,   -131,   -117,
       0,     92,     81,      0,    -62,    -53,      0,     39,     32,      0,
     -22,    -17,      0,     10,      7,      0,     -3,     -2,      0,      0,
       0,
};

static int16_t sat16(int32_t v)
{
  if (v > 32767)
  {
    return 32767;
  }
  if (v < -32768)
  {
    return -32768;
  }
  return (int16_t)v;
}

void pdm_pcm_init(pdm_pcm_t *st, uint16_t slot_mask)
{
  memset(st, 0, sizeof(*st));
  st->slot_mask = slot_mask;
  st->dc_fast   = PDM_WARMUP_SAMPLES;
  st->pcm_mute  = PDM_WARMUP_SAMPLES + FIR_TAPS; /* fast-DC phase + its FIR tail */
  /* The mask's bits in the order of reception (b 15..0). A and B carry exactly
     PDM_SLOT_BITS (checked in pdm_pcm.h); of any other mask the first ones count. */
  uint32_t n = 0;
  for (int b = 15; b >= 0 && n < PDM_SLOT_BITS; b--)
  {
    if ((slot_mask >> b) & 1u)
    {
      st->slot_bit[n++] = (uint8_t)b;
    }
  }
  for (; n < PDM_SLOT_BITS; n++) /* a mask with fewer bits repeats its last one */
  {
    st->slot_bit[n] = (n > 0u) ? st->slot_bit[n - 1u] : 15u;
  }
}

/* One ring half (8192 halfwords = 131072 PDM bits) -> PCM_SAMPLES_PER_HALF samples.
   CIC5 D=64 (gain 64^5 = 2^30, after >>12 same scale as the earlier CIC3)
   -> DC blocker (tau ~43 ms) -> gain with saturation -> FIR 8 kHz low-pass (q15).
   Halfword bits MSB-first = order of reception. */
void pdm_pcm_process_half(pdm_pcm_t *st, const uint16_t *src, int16_t *dst)
{
  int16_t *x = &st->fir_x[FIR_TAPS - 1u];

  /* The CIC state lives in registers for the whole half and wraps modulo 2^32
     (see pdm_pcm.h); on the M33 that is several times faster than 64-bit
     integrators kept in the struct, with bit-identical output. */
  uint32_t i1 = st->cic_i1, i2 = st->cic_i2, i3 = st->cic_i3, i4 = st->cic_i4, i5 = st->cic_i5;
  uint32_t c1 = st->cic_c1, c2 = st->cic_c2, c3 = st->cic_c3, c4 = st->cic_c4, c5 = st->cic_c5;
  const uint8_t *bit = st->slot_bit;
  _Static_assert(PDM_SLOT_BITS == 8u, "the CIC step below is unrolled for 8 slot bits");
  _Static_assert(PDM_CIC_ORDER == 5u && PDM_CIC_DECIM_LOG2 == 6u,
                 "the code below is a CIC5 D=64 (five stages, 8 halfwords, >> 12)");

  for (uint32_t s = 0; s < PCM_SAMPLES_PER_HALF; s++)
  {
    /* 8 halfwords = 128 stream bits, of which 64 belong to the selected mic
       (slot mask) = 1 output sample (D=64). Bit order b 15..0 is the order of
       reception; slot_bit lists the selected mic's bits in that order. */
    for (uint32_t w = 0; w < 8u; w++)
    {
      const uint32_t hw = *src++;
#define PDM_CIC_STEP(j)                       \
      i1 += (((hw >> bit[j]) & 1u) << 1) - 1u; \
      i2 += i1;                               \
      i3 += i2;                               \
      i4 += i3;                               \
      i5 += i4;
      PDM_CIC_STEP(0) PDM_CIC_STEP(1) PDM_CIC_STEP(2) PDM_CIC_STEP(3)
      PDM_CIC_STEP(4) PDM_CIC_STEP(5) PDM_CIC_STEP(6) PDM_CIC_STEP(7)
#undef PDM_CIC_STEP
    }
    const uint32_t y1 = i5 - c1; c1 = i5;
    const uint32_t y2 = y1 - c2; c2 = y1;
    const uint32_t y3 = y2 - c3; c3 = y2;
    const uint32_t y4 = y3 - c4; c4 = y3;
    const uint32_t y5 = y4 - c5; c5 = y4;
    int32_t y = ((int32_t)y5) >> 12; /* D=64: +-2^30 -> +-2^18 */

    /* The first CIC outputs are still comb settling - seed the DC blocker from
       them (final seed = 8th sample, comb settled after 5); otherwise the ramp
       seed decays with tau 43 ms and sets the recording's peak. */
    if (st->dc_seeded < 8)
    {
      st->dc_seeded++;
      st->dc_acc = y << 11;
      x[s] = 0;
      continue;
    }
    /* DC blocker: v = y - (dc_acc>>11), dc_acc += v. The accumulator form has
       no residue from the truncating shift. dc_acc max 2^18 * 2^11 = 2^29.
       During warm-up the step is 32x (tau 64 samples) so the estimate follows
       the mic while it settles; |v| <= 2^19, so 32 * v stays far from overflow. */
    int32_t v = y - (st->dc_acc >> 11);
    if (st->dc_fast)
    {
      st->dc_fast--;
      st->dc_acc += v * 32;
    }
    else
    {
      st->dc_acc += v;
    }
    /* The gain goes in here, before anything rounds: one step of v is two
       output LSBs, so 2v keeps every bit the CIC delivered. Applied after the
       FIR's >>15 it would multiply an already rounded value and leave the low
       4 bits of every sample zero. |v * PCM_GAIN| <= 2^23; sat16 clips at full
       scale = |v| 2^14, and the FIR accumulator bound above still holds. */
    x[s] = sat16((v * PCM_GAIN) >> 3);
  }
  st->cic_i1 = i1; st->cic_i2 = i2; st->cic_i3 = i3; st->cic_i4 = i4; st->cic_i5 = i5;
  st->cic_c1 = c1; st->cic_c2 = c2; st->cic_c3 = c3; st->cic_c4 = c4; st->cic_c5 = c5;

  for (uint32_t s = 0; s < PCM_SAMPLES_PER_HALF; s++)
  {
    const int16_t *xs = &st->fir_x[s];
    int32_t acc = 0;
    for (uint32_t k = 0; k < FIR_TAPS; k++)
    {
      acc += (int32_t)fir_lp[k] * xs[k];
    }
    if (st->pcm_mute)
    {
      st->pcm_mute--;
      dst[s] = 0;
    }
    else
    {
      dst[s] = sat16(acc >> 15);
    }
  }

  /* Keep the last FIR_TAPS-1 samples as history for the next block. */
  memmove(st->fir_x, &st->fir_x[PCM_SAMPLES_PER_HALF],
          (FIR_TAPS - 1u) * sizeof(int16_t));
}
