/**
 * @file classifier_pair.c
 * @brief The OR pair; see classifier_pair.h.
 */
#include "classifier_pair.h"

#include <math.h>

/* Member A's z of the job in flight (begin -> result). */
static float s_job_za = -INFINITY;
static float s_a_in[PAIR_A_MAX];

/* z of c top scores at or above: in double and then float32, as pair.py computes it. */
static float z_of(const pair_ztable_t *t, uint32_t c)
{
    return (float)(-log10(((double)c + 1.0) / ((double)t->n_neg + 1.0)));
}

float pair_z(const pair_ztable_t *t, float s)
{
    if (isnan(s) || t->k == 0u)
    {
        return -INFINITY;
    }
    /* How many top scores are >= s: a prefix of the descending table. */
    uint32_t lo = 0u, hi = t->k;
    while (lo < hi)
    {
        const uint32_t mid = (lo + hi) / 2u;
        if (t->top[mid] >= s)
        {
            lo = mid + 1u;
        }
        else
        {
            hi = mid;
        }
    }
    if (lo == 0u && t->gain > 0.0f)
    {
        return z_of(t, 0u) + (s - t->top[0]) * t->gain;
    }
    return z_of(t, lo);
}

/* Member A on its slice, NaN filled as its training filled it (NaN out if it
   has no fill: the member abstains). */
static float member_a(const pair_t *p, const float *features)
{
    const classifier_t *a = p->a;
    const float        *x = features + p->base_a + a->feature_offset;
    if (a->n_features > PAIR_A_MAX)
    {
        return NAN;
    }
    for (uint32_t i = 0u; i < a->n_features; i++)
    {
        s_a_in[i] = x[i];
        if (isnan(x[i]))
        {
            if (p->impute_a == NULL)
            {
                return NAN;
            }
            s_a_in[i] = p->impute_a[i];
        }
    }
    return a->decide(a->ctx, s_a_in, a->n_features);
}

static bool fits(const pair_t *p, uint16_t n)
{
    return (uint32_t)p->base_a + p->a->feature_offset + p->a->n_features <= n &&
           (uint32_t)p->base_b + p->n_b <= n;
}

float pair_classifier_decide(void *ctx, const float *features, uint16_t n)
{
    const pair_t *p = (const pair_t *)ctx;
    if (!fits(p, n))
    {
        return NAN;
    }
    const float za = pair_z(&p->za, member_a(p, features));
    return fmaxf(za, pair_z(&p->zb, crnn_forward(p->b, features + p->base_b, p->n_b)));
}

static bool job_begin(void *ctx, const float *features, uint16_t n)
{
    const pair_t *p = (const pair_t *)ctx;
    if (!fits(p, n))
    {
        return false;
    }
    s_job_za = pair_z(&p->za, member_a(p, features));
    return crnn_begin(p->b, features + p->base_b, p->n_b);
}

static bool job_run(void *ctx, uint32_t budget)
{
    (void)ctx;
    return crnn_run(budget);
}

static float job_result(void *ctx)
{
    const pair_t *p = (const pair_t *)ctx;
    return fmaxf(s_job_za, pair_z(&p->zb, crnn_result()));
}

const classifier_job_t pair_classifier_job = {
    .begin  = job_begin,
    .run    = job_run,
    .result = job_result,
};
