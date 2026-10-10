/**
 * @file classifier_pair.h
 * @brief Two models on one window, an alarm from either (OR); models/pair.py.
 *
 * Each member's score goes onto its own false-alarm scale from the val negatives,
 * z = -log10((n_ge + 1) / (N + 1)), and the pair decides with the larger z, so one
 * threshold (z x 1000 on the console) spends one budget for both. A table keeps a
 * member's top k negative scores: exact among them, held at z(k) below, continued
 * along the top decade's slope above. Member A (gbt_m1) is scored when the window
 * closes, member B (a network) runs as the job.
 */
#ifndef BOOMDETECT_CLASSIFIER_PAIR_H
#define BOOMDETECT_CLASSIFIER_PAIR_H

#include "classifier.h"
#include "crnn.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The widest member A this build takes (layout 4 minus mean-c0 is 78). */
#define PAIR_A_MAX 96u

typedef struct
{
    const float *top;   /**< k scores, descending */
    uint16_t     k;
    uint32_t     n_neg; /**< N: the val-negative windows the table was taken from */
    float        gain;  /**< z per score unit above top[0]; 0 = none */
} pair_ztable_t;

typedef struct
{
    const classifier_t *a;
    uint16_t            base_a;   /**< where A's layout starts in the pair's vector */
    const float        *impute_a; /**< A's fill for a NaN input, or NULL */
    pair_ztable_t       za;
    const crnn_net_t   *b;
    uint16_t            base_b, n_b;
    pair_ztable_t       zb;
} pair_t;

/** @brief A score on a member's z scale; -INFINITY for NaN (the member abstains). */
float pair_z(const pair_ztable_t *t, float s);

/* classifier_t glue: ctx is the pair_t. */
float pair_classifier_decide(void *ctx, const float *features, uint16_t n);
extern const classifier_job_t pair_classifier_job;

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_CLASSIFIER_PAIR_H */
