/**
 * @file dsp_config.h
 * @brief Configuration constants for Drone Detection DSP pipeline.
 */

#ifndef DSP_CONFIG_H
#define DSP_CONFIG_H

/* Audio Sampling Configuration */
#define SAMPLING_RATE       16000.0f    /* 16 kHz sampling rate */
#define WINDOW_SIZE         1024        /* Number of audio samples per processing window */
#define FFT_SIZE            1024        /* FFT size (must be >= WINDOW_SIZE and power of 2) */

/* MFCC Configuration */
#define NUM_MFCC_COEFFS     13          /* Number of MFCC coefficients to extract */
#define NUM_MEL_FILTERS     20          /* Number of Mel filters in the bank */
#define LOW_FREQ_HZ         0.0f        /* Lower frequency bound for Mel filters */
#define HIGH_FREQ_HZ        8000.0f     /* Upper frequency bound (Nyquist frequency) */

/* Aggregated feature vector one classified window hands to the model:
   [mean(13), std(13), dmean(13), cmax(13)] over 14 MFCC frames, produced by
   det_aggregate() in detector.c. Every model header is checked against this
   at compile time in svm_classifier.c. */
#define DET_FEATURE_COUNT   (4u * NUM_MFCC_COEFFS)

#endif /* DSP_CONFIG_H */
