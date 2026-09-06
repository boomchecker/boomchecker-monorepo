/**
 * @file boomdetect.h
 * @brief Acoustic detection: PCM in, classified windows out.
 *
 * Everything between "here are microphone samples" and "here is a decision"
 * lives here: decimation, the sample FIFO, framing, the RMS squelch, MFCC,
 * feature aggregation and the classifier. What is deliberately NOT here is the
 * microphone, the console and the clock - those are the caller's, which is what
 * lets this run on a host against a WAV file as readily as on the board.
 *
 * The cut is drawn there on purpose. The interesting divergence between what
 * the model was trained on and what the firmware computes is not in the MFCC,
 * which is a library call, but in the windowing policy: training slid windows
 * with a hop of 7 frames over contiguous audio and gated on a window's median
 * level, while the firmware takes disjoint runs of 14 and gates each frame
 * separately, so a window can span a gap of silence. Leaving that policy in the
 * firmware would leave the one thing worth testing untestable.
 *
 * Pacing is the caller's business, and the API forces it to be. Feeding samples
 * and doing work are separate calls because the firmware must not run two MFCCs
 * in one iteration of its superloop: doing so overran the 21.33 ms budget and
 * starved the USB stack. That used to be a comment asking the loop to behave;
 * here it is the shape of the interface.
 */
#ifndef BOOMDETECT_H
#define BOOMDETECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "classifier.h"
#include "dsp_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** MFCC frames aggregated into one classified window. */
#define BOOMDETECT_ACCUM_FRAMES 14u

/** Frame hop, in 16 kHz samples. Frames are WINDOW_SIZE long and overlap. */
#define BOOMDETECT_HOP 512u

/** last_mfcc_slot when the last step computed no MFCC (squelched, or none yet). */
#define BOOMDETECT_NO_MFCC 0xFFFFFFFFu

/**
 * Sample FIFO length, in decimated samples. Must hold one window plus slack:
 * at 48 kHz in and /3 decimation the caller delivers ~341 samples per block
 * against 512 consumed per frame, so the level oscillates rather than grows.
 */
#define BOOMDETECT_RING_LEN 4096u

typedef struct
{
    /** Input decimation factor. 3 for a 48 kHz source feeding a 16 kHz chain. */
    uint16_t decimation;
    /** Per-frame RMS gate in 1/1000 of full scale; 0 disables it. */
    uint32_t squelch_milli;
    /** Decision threshold in 1/1000. May be negative: for an MLP the decision
        is a raw logit, not a probability. */
    int32_t thr_milli;
    /** Which model scores the windows. NULL selects classifier_default(). */
    const classifier_t *classifier;
} boomdetect_config_t;

/** What one processed frame produced. */
typedef struct
{
    uint32_t frame_index; /**< absolute frame counter since init */
    float    rms;         /**< frame level, full scale = 1.0 */
    uint32_t accum;       /**< frames held in the current window after this one */
    bool     squelched;   /**< below the gate, so accumulation was reset */

    /** Set when this frame completed a window; the three fields below are only
        meaningful then. */
    bool     window_complete;
    uint32_t window_start_frame;
    float    decision;
    bool     is_drone;
} boomdetect_event_t;

/**
 * Detector state. Big (about 21 KB, mostly the FIFO), so give it static storage
 * rather than a stack frame. Held by the caller rather than hidden in this
 * translation unit so that a test can run several independently.
 */
typedef struct
{
    boomdetect_config_t cfg;

    float    ring[BOOMDETECT_RING_LEN];
    uint32_t w_idx, r_idx, avail;
    uint32_t dec_phase; /**< index of the next input sample to keep */
    uint32_t dropped;   /**< samples discarded because the FIFO was full */

    float    frame[WINDOW_SIZE]; /**< contiguous copy; the MFCC destroys it */
    float    mfccs[BOOMDETECT_ACCUM_FRAMES * NUM_MFCC_COEFFS];
    float    features[DET_FEATURE_COUNT];
    uint32_t last_mfcc_slot;

    uint32_t accum;
    uint32_t frame_index;
    uint32_t window_start_frame;
    uint32_t windows;
    uint32_t drones;
} boomdetect_t;

/**
 * @brief Reset the detector and adopt @p cfg.
 * @return false if the MFCC front end could not be initialised, or if the
 *         chosen model does not fit the feature layout this build produces.
 *
 * Safe to call repeatedly; the MFCC tables are initialised once and reused.
 */
bool boomdetect_init(boomdetect_t *d, const boomdetect_config_t *cfg);

/**
 * @brief Hand over a block of input samples. Stores only; does no DSP.
 *
 * Samples that do not fit are dropped rather than allowed to overwrite unread
 * ones, and counted (boomdetect_dropped). Overwriting would silently corrupt a
 * window's features, which is far worse than a gap the caller can report.
 */
void boomdetect_push(boomdetect_t *d, const int16_t *pcm, size_t n);

/**
 * @brief Do at most one frame of work.
 * @return true if a frame was processed, and @p out was filled.
 *
 * At most one, never "as much as possible": one MFCC per caller iteration is
 * what keeps the firmware inside its real-time budget. A host test that wants
 * to drain everything simply loops.
 */
bool boomdetect_step(boomdetect_t *d, boomdetect_event_t *out);

/** @brief Samples dropped because the FIFO was full. Nonzero means a gap. */
uint32_t boomdetect_dropped(const boomdetect_t *d);

/** @brief Windows classified, and how many of those were called drone. */
void boomdetect_counts(const boomdetect_t *d, uint32_t *windows, uint32_t *drones);

/**
 * @brief MFCC coefficients of the frame the last successful step() consumed.
 *
 * NUM_MFCC_COEFFS values, valid until the next step(). NULL when the last step
 * computed none - a squelched frame is a successful step that skips the MFCC,
 * and returning the previous frame's coefficients there would hand a parity
 * harness a duplicate it would report as a mismatch of its own.
 *
 * Exists for the parity fixtures, which compare each stage of the chain rather
 * than only its verdict.
 */
const float *boomdetect_last_mfcc(const boomdetect_t *d);

/**
 * @brief The aggregated feature vector of the last completed window.
 *
 * DET_FEATURE_COUNT values in [mean, std, dmean, cmax] x NUM_MFCC_COEFFS order.
 * NULL before the first window completes.
 */
const float *boomdetect_last_features(const boomdetect_t *d);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_H */
