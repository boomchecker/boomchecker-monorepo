/**
 * @file extractor.h
 * @brief The seam between the MFCC frames and whatever shape a model wants.
 *
 * classifier_t decides which MODEL scores a feature vector. This decides what
 * that vector IS. Until it existed, `layout_id` looked like the extension point
 * for a model with a different representation - a CNN over raw frames, say - and
 * was not: boomdetect_init() compared it against one hardcoded constant, so
 * declaring a new layout made init fail. The id was a version tag with exactly
 * one legal value, and the docs said otherwise.
 *
 * There is still exactly one extractor. That is the point of doing this now: a
 * seam is cheap to place while there is one implementation and expensive once
 * there are two and the second one is being wedged in.
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
 * Feature layouts this build knows how to produce.
 *
 * Bump - do not reuse - when the contents or the ORDER of a layout change,
 * never when only a value does. A count cannot catch a reordering: swap two
 * statistics and the width still matches while every weight lands on the wrong
 * input.
 */
#define BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX 1u

/**
 * @brief Turn a window of MFCC frames into a feature vector.
 *
 * @param ctx     the entry's own ctx pointer
 * @param frames  nframes * coeffs floats, frame-major
 * @param nframes frames actually accumulated; never more than the window size
 * @param coeffs  MFCC coefficients per frame
 * @param out     n_features floats
 */
typedef void (*boomdetect_extract_fn)(void *ctx, const float *frames, uint32_t nframes,
                                      uint32_t coeffs, float *out);

typedef struct
{
    /** Stable identifier, for the console and for fixtures. */
    const char *name;
    /** Which layout the vector below is in; models declare the same id. */
    uint16_t layout_id;
    /** Width of the vector this produces. */
    uint16_t n_features;
    boomdetect_extract_fn extract;
    void                 *ctx;
} boomdetect_extractor_t;

/** @brief The extractor a detector uses unless told otherwise. */
const boomdetect_extractor_t *boomdetect_extractor_default(void);

/** @brief Extractor at @p idx, for listing. NULL past the end. */
const boomdetect_extractor_t *boomdetect_extractor_at(size_t idx);

/** @brief How many extractors this image carries. */
size_t boomdetect_extractor_count(void);

/** @brief Look one up by name. NULL if there is no such extractor. */
const boomdetect_extractor_t *boomdetect_extractor_by_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_EXTRACTOR_H */
