/**
 * @file dsp_config.h
 * @brief Shape of the detection chain: frame geometry and feature layouts.
 *
 * Every name here is BOOMDETECT_-prefixed, like the rest of the package's public
 * surface. They were not, and a public header on the firmware's include path
 * defining a bare `FFT_SIZE` is not hypothetical trouble: another board in this
 * repo (fw/bom-node/components/impulse_detection) already defines its own.
 */
#ifndef BOOMDETECT_DSP_CONFIG_H
#define BOOMDETECT_DSP_CONFIG_H

/* Frame geometry. Both lengths are pinned to 1024 by arm_mfcc_init_1024_f32(),
   which hardcodes the FFT length; src/mfcc_processor.c static-asserts it. The
   comment here used to say the FFT "must be >= WINDOW_SIZE", which invited a
   combination that silently overruns: the transform reads and writes fftLen
   floats through a buffer sized from the window length, and in boomdetect_t
   that buffer is followed by the accumulated frames and the features. */
#define BOOMDETECT_SAMPLE_RATE_HZ 16000.0f /* chain rate, after decimation */
#define BOOMDETECT_WINDOW_SIZE    1024     /* samples per analysed frame */
#define BOOMDETECT_FFT_SIZE       1024     /* == BOOMDETECT_WINDOW_SIZE */

/* MFCC shape. These describe what src/mfcc_tables.h was GENERATED with, and
   mfcc_processor.c static-asserts them against the tables' own macros, because
   the two sets are independent and nothing else made them agree.
   The generator also used a 0..8000 Hz mel range. That is not a macro here: it
   is baked into the tables and cannot be changed by editing a header, so
   carrying it as a settable-looking constant was a lie. It is recorded in
   src/mfcc_tables.h's provenance header instead. */
#define BOOMDETECT_MFCC_COEFFS 13
#define BOOMDETECT_MEL_FILTERS 20

/* Frames per classified window, and the firmware's value. The ceiling sizes
   boomdetect_t's frame store; boomdetect_config_t::accum_frames picks the value
   actually used. Lives here rather than in boomdetect.h because the layout-3
   width below is derived from it. */
#define BOOMDETECT_ACCUM_FRAMES_MAX 14u

/* One frame descriptor: what boomdetect_step() keeps per accepted frame for the
   extractors to aggregate (src/extractors.h documents the three blocks).
   [ mfcc x 13 | log-mel x 20 | spectral scalars x 8 ] */
#define BOOMDETECT_FRAME_SCALAR_COUNT 8
#define BOOMDETECT_FRAME_WIDTH \
    (BOOMDETECT_MFCC_COEFFS + BOOMDETECT_MEL_FILTERS + BOOMDETECT_FRAME_SCALAR_COUNT)

/* Feature vector widths, one per layout (include/extractor.h names the ids).
   Layout 1, the deployed one: [mean(13), std(13), dmean(13), cmax(13)], produced
   by src/extractor_stats.c. Every model translation unit under models/
   static-asserts its own slice against the width of the layout it declares. */
#define BOOMDETECT_FEATURE_COUNT (4u * BOOMDETECT_MFCC_COEFFS)
/* Layout 2: layout 1, mean and std of the 8 scalars, log-mel flux. */
#define BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL \
    (BOOMDETECT_FEATURE_COUNT + 2u * BOOMDETECT_FRAME_SCALAR_COUNT + 1u)
/* Layout 3: the log-mel patch, frame-major. */
#define BOOMDETECT_FEATURE_COUNT_LOGMEL (BOOMDETECT_ACCUM_FRAMES_MAX * BOOMDETECT_MEL_FILTERS)
/* What boomdetect_t::features must hold: the widest layout. */
#define BOOMDETECT_FEATURE_MAX BOOMDETECT_FEATURE_COUNT_LOGMEL

#endif /* BOOMDETECT_DSP_CONFIG_H */
