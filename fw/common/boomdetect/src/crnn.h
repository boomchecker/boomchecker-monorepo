/**
 * @file crnn.h
 * @brief The CRNN over the gammatone patch (training/boomdetect_train/models/crnn.py).
 *
 *     patch / patch_std -> [conv 3x3 -> ReLU -> batch norm -> 2x2 max pool] x 2
 *     -> pooled rows as LSTM steps -> last hidden state
 *     [+ aux numbers: NaN -> fill, (x - mean) / std, dense + ReLU] -> dense + ReLU -> logit
 *
 * Float32, held to the PyTorch module by model_parity_test. One static workspace
 * sized for the largest network, so one network runs at a time. A window costs
 * 1.3-2.9 M multiply-accumulates, more than a 21.33 ms mic block has room for, so
 * the network also runs as a job: crnn_run() does at least one step (a pooled row
 * of a convolution, an LSTM step or the head) and stops after about `budget`.
 */
#ifndef BOOMDETECT_CRNN_H
#define BOOMDETECT_CRNN_H

#include <stdbool.h>
#include <stdint.h>

#include "classifier.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The workspace's limits: the networks trained so far, with the 1 s patch. */
#define CRNN_T_MAX          31u
#define CRNN_BANDS          64u
#define CRNN_C1_MAX         32u
#define CRNN_C2_MAX         16u
#define CRNN_HIDDEN_MAX     32u
#define CRNN_AUX_MAX        96u
#define CRNN_AUX_HIDDEN_MAX 32u
#define CRNN_HEAD_MAX       32u

typedef struct
{
    uint16_t t, bands;    /**< patch frames x CRNN_BANDS */
    uint16_t c1, c2;      /**< convolution channels */
    uint16_t hidden;      /**< LSTM state */
    uint16_t n_aux;       /**< aux numbers beside the patch; 0 = none */
    uint16_t aux_hidden;  /**< their dense layer; 0 when n_aux is */
    uint16_t head;        /**< the head's hidden layer */
    uint16_t aux_offset;  /**< index of the first aux number in the feature vector */
    float    patch_std;   /**< the patch is divided by this */
    const float *w1, *b1, *s1, *t1; /**< conv (c1,1,3,3); batch norm after ReLU: s*x + t */
    const float *w2, *b2, *s2, *t2; /**< conv (c2,c1,3,3) */
    const float *w_ih, *w_hh, *b_gates; /**< LSTM (4h,in), (4h,h), b_ih + b_hh; gates i f g o */
    const float *aux_fill, *aux_mean, *aux_std, *w_aux, *b_aux;
    const float *w_head, *b_head, *w_out; /**< (head, hidden + aux_hidden), (head), (head) */
    float        b_out;
} crnn_net_t;

/** @brief Copy the patch and aux numbers of @p features and start a job (abandoning
           any other). false if the descriptor is outside the workspace or @p n short. */
bool crnn_begin(const crnn_net_t *net, const float *features, uint16_t n);

/** @brief Continue the job by at least one step and about @p budget multiply-
           accumulates. true once the logit is ready. */
bool crnn_run(uint32_t budget);

/** @brief The finished job's logit; NAN before. */
float crnn_result(void);

/** @brief All at once; NAN when begin refuses. Not while a job is in flight. */
float crnn_forward(const crnn_net_t *net, const float *features, uint16_t n);

/* classifier_t glue: ctx is the crnn_net_t. */
float crnn_classifier_decide(void *ctx, const float *features, uint16_t n);
extern const classifier_job_t crnn_classifier_job;

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_CRNN_H */
