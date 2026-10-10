/**
 * @file gammatone.h
 * @brief The gammatone band spectrogram behind the CRNN layouts; see dsp_config.h.
 *
 * Per frame: periodic Hann, a real FFT, |X|^2 against the 64 filters of
 * src/gammatone_table.h, divided by the window's energy and logged -
 * training/boomdetect_train/dsp/spectro.py's "gs1k". Its own FFT: the MFCC's is
 * Hamming-windowed, and a different window is a different spectrogram.
 */
#ifndef BOOMDETECT_GAMMATONE_H
#define BOOMDETECT_GAMMATONE_H

#include "boomdetect.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Build the FFT instance and the window once. false if CMSIS refuses. */
bool boomdetect_gs_prepare(void);

/**
 * @brief One frame's BOOMDETECT_GS_BANDS log band powers.
 * @param frame BOOMDETECT_WINDOW_SIZE samples; read only
 * @param row   BOOMDETECT_GS_BANDS values out
 */
void boomdetect_gs_row(const float *frame, float *row);

/** @brief Empty the ring: after init, and after a gap (the rows no longer join up). */
void boomdetect_gs_reset(boomdetect_t *d);

/** @brief Append @p frame's row to the ring, dropping the oldest once it is full. */
void boomdetect_gs_push(boomdetect_t *d, const float *frame);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_GAMMATONE_H */
