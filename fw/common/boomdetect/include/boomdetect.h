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
 * Pacing is the caller's business. Feeding samples and doing work are separate
 * calls, and step() does at most one frame, because the firmware must not run
 * two MFCCs in one iteration of its superloop: doing so overran the 21.33 ms
 * budget and starved the USB stack.
 *
 * Precisely what that buys, since an earlier version of this comment overstated
 * it: the API bounds the CALL, not the loop. Nothing here stops a caller writing
 * `while (boomdetect_step(...))`, and one deliberately does - the fixture
 * generator drains fully, because there is no real-time budget on a synthetic
 * signal. The firmware's own detect path keeps its `if`. What changed is that
 * "one frame" is now a contract rather than a comment asking the loop to behave.
 */
#ifndef BOOMDETECT_H
#define BOOMDETECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "classifier.h"
#include "dsp_config.h"
#include "extractor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Largest number of frames that can be aggregated into one window, and the
 * firmware's value. A ceiling rather than the setting itself: it sizes
 * boomdetect_t::mfccs, while boomdetect_config_t::accum_frames picks the value
 * actually used, so a host run can reproduce the training pipeline's windowing
 * without a rebuild. Defined in dsp_config.h because the log-mel layout's
 * width is derived from it; this is the name the rest of the API uses.
 */
#define BOOMDETECT_ACCUM_FRAMES BOOMDETECT_ACCUM_FRAMES_MAX

/**
 * Default frame hop, in 16 kHz samples. Frames are BOOMDETECT_WINDOW_SIZE long and
 * overlap; boomdetect_config_t::hop overrides it.
 */
#define BOOMDETECT_HOP 512u

/** How a window is gated. The two are not equivalent; see the skew table in
    docs/firmware/detection/index.md. */
typedef enum
{
    /** Every frame must clear the gate, and one that does not resets the
        accumulator. The firmware's policy, and what the deployed model met on
        hardware. */
    BOOMDETECT_GATE_PER_FRAME = 0,
    /** No frame is rejected; the window is kept or dropped as a whole on the
        median of its frames' RMS. What the training pipeline did. */
    BOOMDETECT_GATE_WINDOW_MEDIAN = 1
} boomdetect_gate_t;

/**
 * thr_milli value meaning "whatever the chosen model's operating point is".
 *
 * The default is a property of the model - a linear SVM's decisions live around
 * +-3 while an MLP's are unbounded logits - so every consumer that wanted the
 * model's own threshold had to reach into classifier_t and copy it. One of them
 * forgetting is a detector that never fires, with nothing to say so.
 */
#define BOOMDETECT_THR_MODEL_DEFAULT INT32_MIN

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
    /** RMS gate in 1/1000 of full scale; 0 disables it. Read per frame or per
        window depending on `gate`. */
    uint32_t squelch_milli;
    /** Decision threshold in 1/1000. May be negative: for an MLP the decision
        is a raw logit, not a probability. BOOMDETECT_THR_MODEL_DEFAULT takes
        the chosen model's own operating point, which is almost always what a
        caller means. */
    int32_t thr_milli;
    /** Which model scores the windows. NULL selects classifier_default(). */
    const classifier_t *classifier;
    /** What shape the model is handed. NULL selects
        boomdetect_extractor_default(). init() checks the model's layout_id
        against THIS rather than against a constant, which is what makes a new
        feature representation an addition rather than an edit to the pipeline. */
    const boomdetect_extractor_t *extractor;

    /* The three below are what make the train/deploy skew expressible rather
       than merely describable. Leave them 0 for the firmware's behaviour;
       boomdetect_init() fills in the defaults. */

    /** Frames per window, 1..BOOMDETECT_ACCUM_FRAMES. 0 means the default. */
    uint16_t accum_frames;
    /** Frame hop in 16 kHz samples, 1..BOOMDETECT_WINDOW_SIZE. 0 means BOOMDETECT_HOP.
        The training pipeline slid by 7 frames over contiguous audio; the
        firmware takes disjoint runs. */
    uint16_t hop;
    /** Gating policy. */
    boomdetect_gate_t gate;
} boomdetect_config_t;

/** What one processed frame produced. */
typedef struct
{
    uint32_t frame_index; /**< absolute frame counter since init */
    float    rms;         /**< frame level, full scale = 1.0 */
    uint32_t accum;       /**< frames held in the current window after this one */
    bool     squelched;   /**< below the gate, so accumulation was reset */
    bool     gap;         /**< samples were dropped since the previous frame, so
                               this one is not continuous with it */

    /**
     * The window this frame completed, if it completed one.
     *
     * Nested rather than flat so that reading a decision without checking
     * `complete` is a visible act. Flat, every field was always readable and
     * always zero-valued in between, which made a squelched frame report
     * `is_drone = false` - indistinguishable from "classified, not a drone".
     * `decision` is NAN whenever `complete` is false, for the same reason.
     */
    struct
    {
        bool     complete;
        uint32_t start_frame;
        /** Frame that closed the window. Not `start_frame + accum_frames - 1`:
            under BOOMDETECT_GATE_PER_FRAME a squelched frame resets the
            accumulator, so a window can span silence and be arbitrarily longer
            than the frames it contains. Timestamping from `start_frame` alone
            can therefore land far from where the decision was made. */
        uint32_t end_frame;
        float    decision;
        bool     is_drone;
    } window;
} boomdetect_event_t;

/**
 * Detector state. Big (about 21 KB, mostly the FIFO), so give it static storage
 * rather than a stack frame. Held by the caller rather than hidden in this
 * translation unit rather than a hidden singleton, so several detectors can be
 * driven side by side - pipeline_test does exactly that.
 *
 * "Independently" has one limit worth knowing: the CMSIS MFCC instance and its
 * scratch buffer are file-static in src/mfcc_processor.c, shared by every
 * detector. Sequential use is fine and that is all any consumer does; two
 * threads stepping two detectors at once is not, and moving them in here would
 * cost 8 KB per detector on a part with 640 KB.
 */
typedef struct
{
    boomdetect_config_t cfg;

    float    ring[BOOMDETECT_RING_LEN];
    uint32_t w_idx, r_idx, avail;
    uint32_t dec_phase; /**< index of the next input sample to keep */
    uint32_t dropped;   /**< samples discarded because the FIFO was full */
    bool     gap_pending; /**< a drop happened; the next frame reports it */

    float    frame[BOOMDETECT_WINDOW_SIZE]; /**< contiguous copy; the MFCC destroys it */
    /** One descriptor row per accepted frame of the current window:
        [mfcc x 13 | log-mel x 20 | spectral scalars x 8] (src/extractors.h).
        The name predates the log-mel and scalar blocks; the MFCC still comes
        first in every row, so boomdetect_last_mfcc() is unchanged. */
    float    mfccs[BOOMDETECT_ACCUM_FRAMES * BOOMDETECT_FRAME_WIDTH];
    /** Per-frame RMS of the frames held in the current window, for
        BOOMDETECT_GATE_WINDOW_MEDIAN. Unused by the per-frame gate. */
    float    rms_hist[BOOMDETECT_ACCUM_FRAMES];
    /** The configured extractor's output; only its n_features are meaningful. */
    float    features[BOOMDETECT_FEATURE_MAX];
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
 * @return decimated samples actually stored.
 *
 * Samples that do not fit are dropped rather than allowed to overwrite unread
 * ones, and counted (boomdetect_dropped). Overwriting would silently corrupt a
 * window's features, which is far worse than a gap the caller can report.
 *
 * The FIFO holds BOOMDETECT_RING_LEN *decimated* samples, so the ceiling on one
 * push is that times `decimation`: at the firmware's /3 that is ~12288 input
 * samples, but a host feeding a 16 kHz WAV with decimation 1 must chunk at
 * 4096 or lose the excess. The return value and the `gap` event flag are how a
 * replay harness notices; polling the cumulative boomdetect_dropped() cannot
 * say which window straddled the loss.
 */
size_t boomdetect_push(boomdetect_t *d, const int16_t *pcm, size_t n);

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

/**
 * @brief Absolute frame counter since init.
 *
 * Exists because the port was reading `d->frame_index` straight out of the
 * struct while every other read went through an accessor. The struct is public
 * so both compile; only one of them survives a field being renamed.
 */
uint32_t boomdetect_frame_index(const boomdetect_t *d);

/**
 * @brief Start time of @p frame_index, in milliseconds since init.
 *
 * The chain rate and the hop both live here, so a consumer that wants a
 * timestamp does not have to hardcode either. The port used to divide by a
 * literal 16, which was right only while the hop and the sample rate both
 * stayed at their defaults - and one of them is now a config field.
 */
uint32_t boomdetect_frame_to_ms(const boomdetect_t *d, uint32_t frame_index);

/** @brief Windows classified, and how many of those were called drone. */
void boomdetect_counts(const boomdetect_t *d, uint32_t *windows, uint32_t *drones);

/**
 * @brief MFCC coefficients of the frame the last successful step() consumed.
 *
 * BOOMDETECT_MFCC_COEFFS values, valid until the next step(). NULL when the last step
 * computed none - a squelched frame is a successful step that skips the MFCC,
 * and returning the previous frame's coefficients there would hand a parity
 * harness a duplicate it would report as a mismatch of its own.
 *
 * Exists for the parity fixtures, which compare each stage of the chain rather
 * than only its verdict.
 */
const float *boomdetect_last_mfcc(const boomdetect_t *d);

/**
 * @brief The whole descriptor row of the frame the last successful step() consumed.
 *
 * BOOMDETECT_FRAME_WIDTH values: the MFCC coefficients, then the log-mel
 * vector, then the spectral scalars (src/extractors.h gives the offsets). NULL
 * under the same conditions as boomdetect_last_mfcc(). Exists for the parity
 * fixtures of the layouts that read more than the coefficients.
 */
const float *boomdetect_last_frame(const boomdetect_t *d);

/**
 * @brief The aggregated feature vector of the last completed window.
 *
 * BOOMDETECT_FEATURE_COUNT values in [mean, std, dmean, cmax] x BOOMDETECT_MFCC_COEFFS order.
 * NULL before the first window completes.
 */
const float *boomdetect_last_features(const boomdetect_t *d);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_H */
