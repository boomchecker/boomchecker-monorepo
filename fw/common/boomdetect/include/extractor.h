/**
 * @file extractor.h
 * @brief The seam between the per-frame descriptors and whatever shape a model wants.
 *
 * classifier_t decides which MODEL scores a feature vector. This decides what
 * that vector IS. Until it existed, `layout_id` looked like the extension point
 * for a model with a different representation - a CNN over raw frames, say - and
 * was not: boomdetect_init() compared it against one hardcoded constant, so
 * declaring a new layout made init fail. The id was a version tag with exactly
 * one legal value, and the docs said otherwise.
 *
 * Adding one is a file under src/, a new BOOMDETECT_LAYOUT_* id, and a line in
 * the extractor registry. The pipeline gains nothing: it already asks the
 * configured extractor for its layout and checks the model against THAT rather
 * than against a constant.
 */
#ifndef BOOMDETECT_EXTRACTOR_H
#define BOOMDETECT_EXTRACTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Feature layouts this build knows how to produce. The training package
 * (training/boomdetect_train/features.py) carries the same ids and is the
 * specification of each; the parity fixtures hold the two together.
 *
 * Bump - do not reuse - when the contents or the ORDER of a layout change,
 * never when only a value does. A count cannot catch a reordering: swap two
 * statistics and the width still matches while every weight lands on the wrong
 * input.
 */
#define BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX 1u /* "stats", 52 */
#define BOOMDETECT_LAYOUT_STATS_SPECTRAL      2u /* "stats_spectral", 69 */
#define BOOMDETECT_LAYOUT_LOGMEL              3u /* "logmel", 280 */
#define BOOMDETECT_LAYOUT_STATS_SPECTRAL_MOD  4u /* "stats_spectral_mod", 79 */

/**
 * What the pipeline knows beyond the window's own frames: the envelope ring
 * (dsp_config.h, BOOMDETECT_ENV_*), which outlives any window. Layouts 1-3
 * ignore it; layout 4 reads its modulation spectrum. Passed by pointer so a
 * future side channel is one more field here rather than another parameter.
 */
typedef struct
{
    /** BOOMDETECT_ENV_RING envelope samples at 1 kHz, a ring: the oldest is
        env[env_head], the newest env[(env_head + env_fill - 1) % ring]. */
    const float *env;
    uint32_t     env_head;
    /** Samples the ring holds; BOOMDETECT_ENV_RING once it is full. */
    uint32_t     env_fill;
} boomdetect_side_t;

/**
 * @brief Turn a window of frame descriptors into a feature vector.
 *
 * @param ctx     the entry's own ctx pointer
 * @param frames  nframes rows of `stride` floats; row f starts at frames[f*stride].
 *                The first BOOMDETECT_MFCC_COEFFS of a row are that frame's MFCC,
 *                then the log-mel vector, then the spectral scalars
 *                (src/extractors.h gives the offsets).
 * @param nframes frames actually accumulated; never more than the window size
 * @param stride  floats per row (BOOMDETECT_FRAME_WIDTH today; passed so an
 *                extractor never hardcodes it)
 * @param side    the pipeline's side channel; never NULL from boomdetect_step(),
 *                and holding at least `env_required` samples when it calls
 * @param out     n_features floats
 */
typedef void (*boomdetect_extract_fn)(void *ctx, const float *frames, uint32_t nframes,
                                      uint32_t stride, const boomdetect_side_t *side,
                                      float *out);

typedef struct
{
    /** Stable identifier, for the console and for fixtures. */
    const char *name;

    /** Which layout the vector below is in; models declare the same id. */
    uint16_t layout_id;

    /** Width of the vector this produces. */
    uint16_t n_features;

    /** Envelope samples the side channel must hold before this extractor can
        run; 0 for the layouts that never look at it. boomdetect_step() lets a
        window close without a decision until the ring has that many, which is
        the first two seconds after init or after a gap - the training package
        has NaN there and never fits a model on those windows. */
    uint16_t env_required;

    /** One-time set-up - a transform instance, a window table - done by
        boomdetect_init() rather than inside the first window, which on the
        board closes inside the real-time loop. NULL when there is nothing to
        prepare; returning false fails init. */
    bool (*prepare)(void *ctx);

    boomdetect_extract_fn extract;
    void                 *ctx;
} boomdetect_extractor_t;

/** @brief The registry's first entry ("stats"); init() picks by the model's layout. */
const boomdetect_extractor_t *boomdetect_extractor_default(void);

/** @brief Extractor at @p idx, for listing. NULL past the end. */
const boomdetect_extractor_t *boomdetect_extractor_at(size_t idx);

/** @brief How many extractors this image carries. */
size_t boomdetect_extractor_count(void);

/** @brief Look one up by name. NULL if there is no such extractor. */
const boomdetect_extractor_t *boomdetect_extractor_by_name(const char *name);

/** @brief The extractor producing @p layout_id, or NULL if this build has none. */
const boomdetect_extractor_t *boomdetect_extractor_for_layout(uint16_t layout_id);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_EXTRACTOR_H */
