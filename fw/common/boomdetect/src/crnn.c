/**
 * @file crnn.c
 * @brief The CRNN interpreter; see crnn.h.
 *
 * Tensors are (c, h, w) like PyTorch's. The module's order - ReLU, batch norm,
 * pool - is kept, so nothing depends on the sign of a batch-norm scale.
 */
#include "crnn.h"

#include <math.h>
#include <string.h>

#define KSZ         3u
#define H1_MAX      (CRNN_T_MAX / 2u)     /* pooled rows after the first block */
#define W1          (CRNN_BANDS / 2u)     /* pooled columns after the first block */
#define H2_MAX      (H1_MAX / 2u)         /* LSTM steps */
#define W2          (W1 / 2u)
#define LSTM_IN_MAX (CRNN_C2_MAX * W2)
#define FEAT_MAX    (CRNN_HIDDEN_MAX + CRNN_AUX_HIDDEN_MAX)

enum
{
    STAGE_CONV1 = 0,
    STAGE_CONV2,
    STAGE_LSTM,
    STAGE_HEAD,
    STAGE_DONE
};

/* One job at a time, its state and activations static: far too big for the
   superloop's stack. */
static struct
{
    const crnn_net_t *net;
    uint32_t          stage;
    uint32_t          pos;
    bool              active;
    float             result;
} s_job;

static float s_in[CRNN_T_MAX * CRNN_BANDS];
static float s_p1[CRNN_C1_MAX * H1_MAX * W1];
static float s_p2[CRNN_C2_MAX * H2_MAX * W2];
static float s_aux[CRNN_AUX_MAX];
static float s_h[CRNN_HIDDEN_MAX];
static float s_c[CRNN_HIDDEN_MAX];
static float s_x[LSTM_IN_MAX];
static float s_g[4u * CRNN_HIDDEN_MAX];
static float s_feat[FEAT_MAX];
static float s_rows[2u * CRNN_BANDS];

/* The shape must fit the workspace; the weights are the generator's, so only the
   optional aux block's are checked for presence. */
static bool net_ok(const crnn_net_t *net)
{
    return net != NULL && net->t >= 4u && net->t <= CRNN_T_MAX && net->bands == CRNN_BANDS &&
           net->c1 >= 1u && net->c1 <= CRNN_C1_MAX && net->c2 >= 1u && net->c2 <= CRNN_C2_MAX &&
           net->hidden >= 1u && net->hidden <= CRNN_HIDDEN_MAX && net->n_aux <= CRNN_AUX_MAX &&
           net->aux_hidden <= CRNN_AUX_HIDDEN_MAX && net->head >= 1u &&
           net->head <= CRNN_HEAD_MAX && net->patch_std > 0.0f &&
           (net->n_aux == 0u) == (net->aux_hidden == 0u) &&
           (net->n_aux == 0u || (net->aux_fill != NULL && net->aux_mean != NULL &&
                                 net->aux_std != NULL && net->w_aux != NULL));
}

static float sigmoidf(float x)
{
    return 1.0f / (1.0f + expf(-x));
}

static float dot(const float *a, const float *b, uint32_t n)
{
    float acc = 0.0f;
    for (uint32_t i = 0u; i < n; i++)
    {
        acc += a[i] * b[i];
    }
    return acc;
}

/* Row r of one output channel of a 3x3 'same' convolution: out[x] for x < w,
   from `cin` planes of (h, w) and that channel's (cin, 3, 3) weights. The edge
   columns are split out so the inner loop has no bounds tests. */
static void conv_row(const float *in, uint32_t cin, uint32_t h, uint32_t w, const float *wo,
                     float bias, uint32_t r, float *out)
{
    for (uint32_t x = 0u; x < w; x++)
    {
        out[x] = bias;
    }
    for (uint32_t i = 0u; i < cin; i++)
    {
        const float *plane = in + (size_t)i * h * w;
        const float *k     = wo + (size_t)i * KSZ * KSZ;
        for (uint32_t ky = 0u; ky < KSZ; ky++)
        {
            const int32_t iy = (int32_t)r + (int32_t)ky - 1;
            if (iy < 0 || iy >= (int32_t)h)
            {
                continue;
            }
            const float *src = plane + (size_t)iy * w;
            const float  k0  = k[ky * KSZ];
            const float  k1  = k[ky * KSZ + 1u];
            const float  k2  = k[ky * KSZ + 2u];
            out[0] += k1 * src[0] + k2 * src[1];
            for (uint32_t x = 1u; x + 1u < w; x++)
            {
                out[x] += k0 * src[x - 1u] + k1 * src[x] + k2 * src[x + 1u];
            }
            out[w - 1u] += k0 * src[w - 2u] + k1 * src[w - 1u];
        }
    }
}

/* Conv rows 2y and 2y+1 of one output channel -> ReLU -> batch norm -> the
   pooled row y (w / 2 values). */
static void conv_relu_bn_pool(const float *in, uint32_t cin, uint32_t h, uint32_t w,
                              const float *wo, float bias, float s, float t, uint32_t y,
                              float *pooled)
{
    float *r0 = s_rows;
    float *r1 = s_rows + w;
    conv_row(in, cin, h, w, wo, bias, 2u * y, r0);
    conv_row(in, cin, h, w, wo, bias, 2u * y + 1u, r1);
    for (uint32_t x = 0u; x < w; x++)
    {
        r0[x] = s * ((r0[x] > 0.0f) ? r0[x] : 0.0f) + t;
        r1[x] = s * ((r1[x] > 0.0f) ? r1[x] : 0.0f) + t;
    }
    for (uint32_t x = 0u; x < w / 2u; x++)
    {
        pooled[x] = fmaxf(fmaxf(r0[2u * x], r0[2u * x + 1u]), fmaxf(r1[2u * x], r1[2u * x + 1u]));
    }
}

/* One step of the job; returns the multiply-accumulates it cost. */
static uint32_t step(void)
{
    const crnn_net_t *net = s_job.net;
    const uint32_t    h1  = net->t / 2u;
    const uint32_t    h2  = h1 / 2u;
    switch (s_job.stage)
    {
    case STAGE_CONV1:
    {
        const uint32_t y = s_job.pos;
        for (uint32_t o = 0u; o < net->c1; o++)
        {
            conv_relu_bn_pool(s_in, 1u, net->t, net->bands, net->w1 + (size_t)o * KSZ * KSZ,
                              net->b1[o], net->s1[o], net->t1[o], y,
                              s_p1 + ((size_t)o * h1 + y) * W1);
        }
        if (++s_job.pos >= h1)
        {
            s_job.stage = STAGE_CONV2;
            s_job.pos   = 0u;
        }
        return 2u * net->bands * net->c1 * KSZ * KSZ;
    }
    case STAGE_CONV2:
    {
        const uint32_t y = s_job.pos / net->c2;
        const uint32_t o = s_job.pos % net->c2;
        conv_relu_bn_pool(s_p1, net->c1, h1, W1, net->w2 + (size_t)o * net->c1 * KSZ * KSZ,
                          net->b2[o], net->s2[o], net->t2[o], y,
                          s_p2 + ((size_t)o * h2 + y) * W2);
        if (++s_job.pos >= h2 * net->c2)
        {
            s_job.stage = STAGE_LSTM;
            s_job.pos   = 0u;
        }
        return 2u * W1 * net->c1 * KSZ * KSZ;
    }
    case STAGE_LSTM:
    {
        const uint32_t k   = s_job.pos;
        const uint32_t hid = net->hidden;
        const uint32_t in  = (uint32_t)net->c2 * W2;
        for (uint32_t c = 0u; c < net->c2; c++)
        {
            memcpy(s_x + c * W2, s_p2 + ((size_t)c * h2 + k) * W2, W2 * sizeof(float));
        }
        for (uint32_t g = 0u; g < 4u * hid; g++)
        {
            s_g[g] = net->b_gates[g] + dot(net->w_ih + (size_t)g * in, s_x, in) +
                     dot(net->w_hh + (size_t)g * hid, s_h, hid);
        }
        for (uint32_t j = 0u; j < hid; j++)
        {
            const float ig = sigmoidf(s_g[j]);
            const float fg = sigmoidf(s_g[hid + j]);
            const float gg = tanhf(s_g[2u * hid + j]);
            const float og = sigmoidf(s_g[3u * hid + j]);
            s_c[j]         = fg * s_c[j] + ig * gg;
            s_h[j]         = og * tanhf(s_c[j]);
        }
        if (++s_job.pos >= h2)
        {
            s_job.stage = STAGE_HEAD;
            s_job.pos   = 0u;
        }
        return 4u * hid * (in + hid);
    }
    case STAGE_HEAD:
    {
        const uint32_t hid = net->hidden;
        memcpy(s_feat, s_h, hid * sizeof(float));
        for (uint32_t j = 0u; j < net->aux_hidden; j++)
        {
            const float a = net->b_aux[j] + dot(net->w_aux + (size_t)j * net->n_aux, s_aux, net->n_aux);
            s_feat[hid + j] = (a > 0.0f) ? a : 0.0f;
        }
        const uint32_t nf  = hid + net->aux_hidden;
        float          out = net->b_out;
        for (uint32_t j = 0u; j < net->head; j++)
        {
            const float z = net->b_head[j] + dot(net->w_head + (size_t)j * nf, s_feat, nf);
            out += net->w_out[j] * ((z > 0.0f) ? z : 0.0f);
        }
        s_job.result = out;
        s_job.stage  = STAGE_DONE;
        return (uint32_t)net->aux_hidden * net->n_aux + (uint32_t)net->head * nf + net->head;
    }
    default:
        return 0u;
    }
}

bool crnn_begin(const crnn_net_t *net, const float *features, uint16_t n)
{
    s_job.active = false;
    s_job.result = NAN;
    if (!net_ok(net) || features == NULL)
    {
        return false;
    }
    const uint32_t patch = (uint32_t)net->t * net->bands;
    const uint32_t need  = (net->n_aux > 0u) ? (uint32_t)net->aux_offset + net->n_aux : patch;
    if ((uint32_t)n < need || (net->n_aux > 0u && net->aux_offset < patch))
    {
        return false;
    }
    /* A division, as torchnets.split_inputs() divides, rather than a multiply
       by the reciprocal: it is 2k operations a window and keeps the rounding. */
    for (uint32_t i = 0u; i < patch; i++)
    {
        s_in[i] = features[i] / net->patch_std;
    }
    for (uint32_t i = 0u; i < net->n_aux; i++)
    {
        float v = features[net->aux_offset + i];
        if (isnan(v))
        {
            v = net->aux_fill[i];
        }
        s_aux[i] = (v - net->aux_mean[i]) / net->aux_std[i];
    }
    memset(s_h, 0, sizeof(s_h));
    memset(s_c, 0, sizeof(s_c));
    s_job.net    = net;
    s_job.stage  = STAGE_CONV1;
    s_job.pos    = 0u;
    s_job.active = true;
    return true;
}

bool crnn_run(uint32_t budget)
{
    if (!s_job.active)
    {
        return false;
    }
    uint32_t spent = 0u;
    while (s_job.stage != STAGE_DONE)
    {
        spent += step();
        if (spent >= budget)
        {
            break;
        }
    }
    return s_job.stage == STAGE_DONE;
}

float crnn_result(void)
{
    return (s_job.active && s_job.stage == STAGE_DONE) ? s_job.result : NAN;
}

float crnn_forward(const crnn_net_t *net, const float *features, uint16_t n)
{
    if (!crnn_begin(net, features, n))
    {
        return NAN;
    }
    (void)crnn_run(UINT32_MAX);
    return crnn_result();
}

float crnn_classifier_decide(void *ctx, const float *features, uint16_t n)
{
    return crnn_forward((const crnn_net_t *)ctx, features, n);
}

static bool job_begin(void *ctx, const float *features, uint16_t n)
{
    return crnn_begin((const crnn_net_t *)ctx, features, n);
}

static bool job_run(void *ctx, uint32_t budget)
{
    (void)ctx;
    return crnn_run(budget);
}

static float job_result(void *ctx)
{
    (void)ctx;
    return crnn_result();
}

const classifier_job_t crnn_classifier_job = {
    .begin  = job_begin,
    .run    = job_run,
    .result = job_result,
};
