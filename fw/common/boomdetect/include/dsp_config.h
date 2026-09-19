/**
 * @file dsp_config.h
 * @brief Shape of the detection chain: frame geometry and feature layout.
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
   that buffer is followed by the accumulated MFCC frames and the features. */
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

/* Aggregated feature vector one classified window hands to the model:
   [mean(13), std(13), dmean(13), cmax(13)], produced by aggregate() in
   src/boomdetect.c. Every model translation unit under models/ static-asserts
   its own slice against this. */
#define BOOMDETECT_FEATURE_COUNT (4u * BOOMDETECT_MFCC_COEFFS)

#endif /* BOOMDETECT_DSP_CONFIG_H */
