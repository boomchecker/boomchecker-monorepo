/**
 * @file dsp_config.h
 * @brief Configuration constants for Drone Detection DSP pipeline.
 */

#ifndef DSP_CONFIG_H
#define DSP_CONFIG_H

/* Audio Sampling Configuration */
#define SAMPLING_RATE       16000.0f    /* 16 kHz sampling rate */
#define WINDOW_SIZE         1024        /* Number of audio samples per processing window */
/* Both are pinned to 1024 by arm_mfcc_init_1024_f32(), which hardcodes the FFT
   length; mfcc_processor.c static-asserts it. The old comment here said FFT_SIZE
   "must be >= WINDOW_SIZE", which invited a combination that silently overruns:
   the transform reads and writes fftLen floats through a buffer sized from
   WINDOW_SIZE, and in boomdetect_t that buffer is immediately followed by the
   accumulated MFCC frames and the feature vector. */
#define FFT_SIZE            1024        /* == WINDOW_SIZE, see above */

/* MFCC Configuration */
#define NUM_MFCC_COEFFS     13          /* Number of MFCC coefficients to extract */
#define NUM_MEL_FILTERS     20          /* Number of Mel filters in the bank */
#define LOW_FREQ_HZ         0.0f        /* Lower frequency bound for Mel filters */
#define HIGH_FREQ_HZ        8000.0f     /* Upper frequency bound (Nyquist frequency) */

/* Aggregated feature vector one classified window hands to the model:
   [mean(13), std(13), dmean(13), cmax(13)] over 14 MFCC frames, produced by
   det_aggregate() in detector.c. Every model header is checked against this
   at compile time by each model translation unit under models/. */
#define DET_FEATURE_COUNT   (4u * NUM_MFCC_COEFFS)

#endif /* DSP_CONFIG_H */
