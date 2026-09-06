/**
 * @file extractors.h
 * @brief The extractors this build carries, and the frame descriptor they read.
 *
 * A frame descriptor is one row of BOOMDETECT_FRAME_WIDTH floats per accepted
 * frame, filled by boomdetect_step():
 *
 *     [ mfcc x 13 | log-mel x 20 | spectral scalars x 8 ]
 *
 * The MFCC block comes first so that the deployed layout ("stats", the first
 * 13 of every row) is exactly what it always was and the checked-in fixtures
 * still match bit for bit; the log-mel vector is what the MFCC's DCT was
 * computed from and costs nothing extra; the scalars are frame_scalars.h.
 *
 * Each extractor is a boomdetect_extractor_t in its own translation unit; the
 * registry in extractor_stats.c lists them. training/boomdetect_train/
 * features.py is the specification of every layout and generates the parity
 * fixture that checks these against it.
 */
#ifndef BOOMDETECT_EXTRACTORS_H
#define BOOMDETECT_EXTRACTORS_H

#include "dsp_config.h"
#include "extractor.h"
#include "frame_scalars.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Offsets of the three blocks inside a frame descriptor row. */
#define BOOMDETECT_FRAME_MFCC_OFF   0u
#define BOOMDETECT_FRAME_LOGMEL_OFF ((uint32_t)BOOMDETECT_MFCC_COEFFS)
#define BOOMDETECT_FRAME_SCALAR_OFF ((uint32_t)BOOMDETECT_MFCC_COEFFS + (uint32_t)BOOMDETECT_MEL_FILTERS)

/** Layout 1, the deployed one: [mean, std, dmean, cmax] x 13. */
extern const boomdetect_extractor_t boomdetect_extractor_stats;
/** Layout 2: layout 1, then mean and std of the 8 scalars, then log-mel flux (69). */
extern const boomdetect_extractor_t boomdetect_extractor_stats_spectral;
/** Layout 3: the 14 x 20 log-mel patch minus its mean, frame-major (280). */
extern const boomdetect_extractor_t boomdetect_extractor_logmel;

/**
 * @brief [mean, std, dmean, cmax] of the first `coeffs` values of each row.
 *
 * Shared by layouts 1 and 2 so that the first 52 values of layout 2 are
 * bit-identical to layout 1 - a model trained on layout 1 could, in
 * principle, read the prefix of layout 2, and the tests hold that.
 *
 * @param frames  nframes rows of `stride` floats
 * @param out     4 * coeffs floats
 */
void boomdetect_aggregate_stats(const float *frames, uint32_t nframes, uint32_t stride,
                                uint32_t coeffs, float *out);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_EXTRACTORS_H */
