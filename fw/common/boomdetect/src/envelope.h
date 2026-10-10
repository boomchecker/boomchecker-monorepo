/**
 * @file envelope.h
 * @brief The 1-4 kHz envelope ring behind layout 4; see dsp_config.h.
 *
 * Three biquads run sample by sample over the 16 kHz chain - band-pass, |x|,
 * low-pass, coefficients in envelope_coefs.h as the training package designed
 * them - and every BOOMDETECT_ENV_DECIM-th output goes into boomdetect_t's
 * ring. The chain is causal and the ring follows the audio continuously,
 * gated frames included, so that the modulation spectrum an extractor takes
 * over it is the one dsp/mfcc.py's envelope_1k() and features.py's
 * modulation_stats() computed on the same samples.
 */
#ifndef BOOMDETECT_ENVELOPE_H
#define BOOMDETECT_ENVELOPE_H

#include "boomdetect.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Zero the filter states, the phase and the ring. boomdetect_init() calls it. */
void boomdetect_envelope_reset(boomdetect_t *d);

/**
 * @brief Run @p n chain samples through the envelope and append what the
 *        decimator keeps to the ring. The phase carries across calls, so any
 *        block size tiles the stream exactly once.
 */
void boomdetect_envelope_push(boomdetect_t *d, const float *x, uint32_t n);

/**
 * @brief Samples were lost before the next frame: the ring's contents no longer
 *        describe two continuous seconds, so it starts filling again. The filter
 *        states are kept - a reset there would add a transient of its own.
 */
void boomdetect_envelope_gap(boomdetect_t *d);

/** @brief The side channel boomdetect_step() hands the extractor. */
boomdetect_side_t boomdetect_envelope_side(const boomdetect_t *d);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_ENVELOPE_H */
