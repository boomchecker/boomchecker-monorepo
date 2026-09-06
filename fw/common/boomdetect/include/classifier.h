/**
 * @file classifier.h
 * @brief The seam between the feature pipeline and whatever scores its output.
 *
 * Until now the model was chosen by a single #include and a preprocessor ladder
 * in one translation unit: swapping it meant editing a source file, rebuilding
 * and reflashing, and only ever one model existed in an image. For a project
 * whose point is comparing detectors on the same audio, that is the wrong
 * shape.
 *
 * Each model is now its own translation unit exporting one classifier_t. That
 * works because the weight tables in the generated headers are `static const`,
 * so their identical symbol names have internal linkage and several can coexist
 * in one binary.
 */
#ifndef BOOMDETECT_CLASSIFIER_H
#define BOOMDETECT_CLASSIFIER_H

#include <stddef.h>
#include <stdint.h>

#include "dsp_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Score one feature vector.
 * @param ctx      the entry's own ctx pointer, for models that need state.
 *                 Non-const: a stateful model that had to cast the const away
 *                 to use it would be a worse contract than saying so.
 * @param features already offset by the entry's feature_offset, so this reads
 *                 from index 0 and needs to know nothing about the layout
 * @param n        how many values are readable through @p features. Passed even
 *                 though the entry declares it, because n_features was
 *                 otherwise validated once at init and then never enforced: a
 *                 model reading past its slice stays inside the caller's
 *                 feature array, so it reads the next family's statistics and
 *                 no sanitizer fires.
 * @return the decision value. Its scale is the model's own business: a linear
 *         SVM's lives around +-3, an MLP's is an unbounded logit.
 */
typedef float (*classifier_decide_fn)(void *ctx, const float *features, uint16_t n);

typedef struct
{
    /** Stable identifier used by the console and by test fixtures. */
    const char *name;

    /**
     * Which feature layout this model was trained against.
     *
     * n_features alone cannot catch a reordering: swap two statistics in the
     * aggregate and the count still matches while every weight lands on the
     * wrong input. This is only as strong as the discipline of bumping it when
     * the layout changes, so treat it as a tripwire rather than a proof - the
     * real check is the parity fixture.
     */
    uint16_t layout_id;

    /** How many values, starting at feature_offset, decide() reads. */
    uint16_t n_features;

    /**
     * First feature this model consumes. The MLPs skip index 0, the mean of
     * coefficient 0, because it carries absolute level and they are meant to be
     * gain-invariant; the linear SVMs read the vector from the start.
     */
    uint16_t feature_offset;

    /** Operating point that belongs to THIS model, in 1/1000. */
    int32_t default_thr_milli;

    classifier_decide_fn decide;
    void                *ctx;
} classifier_t;

/** Feature layout produced by the aggregation in boomdetect.c:
    [mean, std, dmean, cmax] x BOOMDETECT_MFCC_COEFFS, in that order.
    Bump when the contents or the order change, never when only a value does. */
#define BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX 1u

/** @brief Look a model up by name. NULL if there is no such model. */
const classifier_t *classifier_by_name(const char *name);

/** @brief Model at @p idx, for listing. NULL past the end. */
const classifier_t *classifier_at(size_t idx);

/** @brief How many models this image carries. */
size_t classifier_count(void);

/** @brief The model used when nothing else is chosen: the deployed one. */
const classifier_t *classifier_default(void);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_CLASSIFIER_H */
