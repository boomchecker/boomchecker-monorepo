/**
 * @file frame_scalars.h
 * @brief Eight per-frame spectral scalars from one magnitude spectrum.
 *
 * These are the things that separate a rotor from a hum and that the MFCC
 * envelope smooths away: how much power sits above 4 kHz, how flat the
 * spectrum is, how well a harmonic comb fits and at which fundamental. They
 * are computed once per accepted frame, from the magnitude spectrum the MFCC
 * leaves behind in the frame buffer, and stored in the frame descriptor for
 * the layout-2 extractor to aggregate (see extractor.h).
 *
 * The arithmetic is specified by the Python in
 * training/boomdetect_train/features.py (frame_scalars); the parity fixture
 * generated from it (tests/vectors/extractor_expected.h) holds the two
 * together. Every constant here has a twin there.
 */
#ifndef BOOMDETECT_FRAME_SCALARS_H
#define BOOMDETECT_FRAME_SCALARS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of scalars boomdetect_frame_scalars() writes. */
#define BOOMDETECT_FRAME_SCALARS 8u

/** Indices into the eight, for readers of the frame descriptor. */
enum
{
    BOOMDETECT_SCALAR_HI_RATIO = 0,   /**< power 4..8 kHz over 0..8 kHz */
    BOOMDETECT_SCALAR_MID_RATIO = 1,  /**< power 1..4 kHz over 0..8 kHz */
    BOOMDETECT_SCALAR_CENTROID = 2,   /**< spectral centroid / 8 kHz */
    BOOMDETECT_SCALAR_FLATNESS = 3,   /**< geometric over arithmetic mean power */
    BOOMDETECT_SCALAR_ROLLOFF = 4,    /**< bin below which 85 % of the power lies / 512 */
    BOOMDETECT_SCALAR_CREST = 5,      /**< max magnitude over mean magnitude */
    BOOMDETECT_SCALAR_HARMONICITY = 6,/**< best harmonic comb sum over total magnitude */
    BOOMDETECT_SCALAR_F0 = 7          /**< that comb's fundamental / 400 Hz */
};

/** Magnitude bins the scalars read: DC through Nyquist of a 1024-point FFT. */
#define BOOMDETECT_SCALAR_BINS 513u

/**
 * @brief Compute the eight scalars.
 * @param mag  BOOMDETECT_SCALAR_BINS magnitudes (bin 0 = DC). Bin 0 is ignored:
 *             the PDM chain leaves an offset there that says nothing about
 *             the sound.
 * @param out  BOOMDETECT_FRAME_SCALARS floats.
 */
void boomdetect_frame_scalars(const float *mag, float *out);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_FRAME_SCALARS_H */
