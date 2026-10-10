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
/* Layout 4: layout 2, then the modulation features of the 1-4 kHz envelope
   (src/extractor_mod.c; features.py MOD_NAMES). */
#define BOOMDETECT_MOD_FEATURES 10u
#define BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL_MOD \
    (BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL + BOOMDETECT_MOD_FEATURES)
/* Layout 8: layout 4, then the same ten numbers over four seconds of envelope.
   Produced only as the aux block of the gammatone hybrids below. */
#define BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL_MOD2S4S \
    (BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL_MOD + BOOMDETECT_MOD_FEATURES)

/* The envelope behind layouts 4 and 8. The 1-4 kHz band of the chain, rectified
   and low-passed (three biquads, src/envelope_coefs.h), kept every
   BOOMDETECT_ENV_DECIM-th sample - 1 kHz, BOOMDETECT_ENV_PER_HOP values per
   512-sample hop - in a ring that boomdetect_t carries across windows: four
   seconds (BOOMDETECT_MOD_FRAMES_LONG hops), of which layout 4 reads the newest
   two (BOOMDETECT_MOD_FRAMES hops, 1.98 s). A hovering drone's rotor noise is
   amplitude-modulated at its blade-pass rate and the ring is what the
   modulation spectrum is taken over. Twins: dsp/mfcc.py ENV_DECIM/ENV_PER_FRAME
   and features.py MOD_FRAMES / MOD_FRAMES_LONG; the spans are in samples so
   that a host run with another hop still integrates the same seconds. */
#define BOOMDETECT_ENV_DECIM        16u
#define BOOMDETECT_ENV_PER_HOP      (512u / BOOMDETECT_ENV_DECIM) /* at the default hop */
#define BOOMDETECT_ENV_SECTIONS     3u                            /* biquads in the chain */
#define BOOMDETECT_MOD_FRAMES       62u
#define BOOMDETECT_MOD_FRAMES_LONG  125u
#define BOOMDETECT_ENV_SPAN_2S      (BOOMDETECT_MOD_FRAMES * BOOMDETECT_ENV_PER_HOP)      /* 1984 */
#define BOOMDETECT_ENV_SPAN_4S      (BOOMDETECT_MOD_FRAMES_LONG * BOOMDETECT_ENV_PER_HOP) /* 4000 */
#define BOOMDETECT_ENV_RING         BOOMDETECT_ENV_SPAN_4S

/* The gammatone band spectrogram ("gs1k", training/boomdetect_train/dsp/spectro.py):
   per frame, a periodic Hann window over the frame's 1024 samples, |FFT|^2 summed
   against BOOMDETECT_GS_BANDS gammatone filters (src/gammatone_table.h) and
   logged. Unlike the MFCC it is taken for every frame, gated or not, into a ring
   of the newest BOOMDETECT_GS_RING_ROWS rows: a network's patch is the window's
   14 frames, or the 31 that end with it (about one second). */
#define BOOMDETECT_GS_BANDS       64u
#define BOOMDETECT_GS_PATCH_SHORT BOOMDETECT_ACCUM_FRAMES_MAX
#define BOOMDETECT_GS_PATCH_LONG  31u
#define BOOMDETECT_GS_RING_ROWS   BOOMDETECT_GS_PATCH_LONG

/* The gammatone layouts (include/extractor.h names the ids): the patch, then for
   the hybrids the numbers of layout 4 or 8 beside it, mean-c0 included. */
#define BOOMDETECT_FEATURE_COUNT_PATCH31_GS1K (BOOMDETECT_GS_PATCH_LONG * BOOMDETECT_GS_BANDS)
#define BOOMDETECT_FEATURE_COUNT_HYBRID31_GS1K \
    (BOOMDETECT_FEATURE_COUNT_PATCH31_GS1K + BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL_MOD)
#define BOOMDETECT_FEATURE_COUNT_HYBRID8_GS1K \
    (BOOMDETECT_GS_PATCH_SHORT * BOOMDETECT_GS_BANDS + BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL_MOD2S4S)
#define BOOMDETECT_FEATURE_COUNT_HYBRID8_31_GS1K \
    (BOOMDETECT_FEATURE_COUNT_PATCH31_GS1K + BOOMDETECT_FEATURE_COUNT_STATS_SPECTRAL_MOD2S4S)

/* What boomdetect_t::features must hold: the widest layout. */
#define BOOMDETECT_FEATURE_MAX BOOMDETECT_FEATURE_COUNT_HYBRID8_31_GS1K

#endif /* BOOMDETECT_DSP_CONFIG_H */
