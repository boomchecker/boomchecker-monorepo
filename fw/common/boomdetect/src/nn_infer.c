/**
 * @file nn_infer.c
 * @brief The CNN interpreter; see nn_infer.h.
 */
#include "nn_infer.h"

#include <string.h>

static bool lay_in(const nn_net_t *net, const float *patch, float *dst)
{
    const nn_layer_t *first = &net->layers[0];
    const uint32_t    n     = (uint32_t)first->in_c * first->in_h * first->in_w;
    if (n != net->patch_len || n > net->max_act)
    {
        return false;
    }
    if (net->input_layout == NN_IN_FRAME_MAJOR)
    {
        memcpy(dst, patch, n * sizeof(float));
        return true;
    }
    if (net->input_layout == NN_IN_MEL_CHANNELS)
    {
        /* patch[t*C + m] -> dst[m*T + t]; H is 1. */
        const uint32_t c = first->in_c, t_len = first->in_w;
        if (first->in_h != 1u || c * t_len != n)
        {
            return false;
        }
        for (uint32_t t = 0u; t < t_len; t++)
        {
            for (uint32_t m = 0u; m < c; m++)
            {
                dst[m * t_len + t] = patch[t * c + m];
            }
        }
        return true;
    }
    return false;
}

static void conv2d(const nn_layer_t *L, const float *in, float *out, bool depthwise)
{
    const int32_t  H = L->in_h, W = L->in_w;
    const uint32_t cpg = depthwise ? 1u : L->in_c; /* input channels per output */
    for (uint32_t o = 0u; o < L->out_c; o++)
    {
        const float *wo = L->w + (size_t)o * cpg * L->kh * L->kw;
        for (int32_t y = 0; y < (int32_t)L->out_h; y++)
        {
            for (int32_t x = 0; x < (int32_t)L->out_w; x++)
            {
                float acc = L->b[o];
                for (uint32_t i = 0u; i < cpg; i++)
                {
                    const uint32_t ic  = depthwise ? o : i;
                    const float   *inp = in + (size_t)ic * H * W;
                    const float   *wi  = wo + (size_t)i * L->kh * L->kw;
                    for (uint32_t ky = 0u; ky < L->kh; ky++)
                    {
                        const int32_t iy = y + (int32_t)ky - (int32_t)L->ph;
                        if (iy < 0 || iy >= H)
                        {
                            continue;
                        }
                        for (uint32_t kx = 0u; kx < L->kw; kx++)
                        {
                            const int32_t ix = x + (int32_t)kx - (int32_t)L->pw;
                            if (ix < 0 || ix >= W)
                            {
                                continue;
                            }
                            acc += wi[ky * L->kw + kx] * inp[iy * W + ix];
                        }
                    }
                }
                out[((size_t)o * L->out_h + (size_t)y) * L->out_w + (size_t)x] = acc;
            }
        }
    }
}

static void pool2(const nn_layer_t *L, const float *in, float *out)
{
    for (uint32_t c = 0u; c < L->out_c; c++)
    {
        const float *inp = in + (size_t)c * L->in_h * L->in_w;
        for (uint32_t y = 0u; y < L->out_h; y++)
        {
            for (uint32_t x = 0u; x < L->out_w; x++)
            {
                const float a = inp[(2u * y) * L->in_w + 2u * x];
                const float b = inp[(2u * y) * L->in_w + 2u * x + 1u];
                const float cc = inp[(2u * y + 1u) * L->in_w + 2u * x];
                const float d = inp[(2u * y + 1u) * L->in_w + 2u * x + 1u];
                float m = a;
                if (b > m) m = b;
                if (cc > m) m = cc;
                if (d > m) m = d;
                out[((size_t)c * L->out_h + y) * L->out_w + x] = m;
            }
        }
    }
}

static void gap(const nn_layer_t *L, const float *in, float *out)
{
    const uint32_t hw = (uint32_t)L->in_h * L->in_w;
    for (uint32_t c = 0u; c < L->in_c; c++)
    {
        const float *inp = in + (size_t)c * hw;
        float        acc = 0.0f;
        for (uint32_t i = 0u; i < hw; i++)
        {
            acc += inp[i];
        }
        out[c] = acc / (float)hw;
    }
}

static void dense(const nn_layer_t *L, const float *in, float *out)
{
    const uint32_t n_in = (uint32_t)L->in_c * L->in_h * L->in_w;
    for (uint32_t o = 0u; o < L->out_c; o++)
    {
        const float *wo  = L->w + (size_t)o * n_in;
        float        acc = L->b[o];
        for (uint32_t i = 0u; i < n_in; i++)
        {
            acc += wo[i] * in[i];
        }
        out[o] = acc;
    }
}

static bool shapes_ok(const nn_net_t *net)
{
    for (uint16_t i = 0u; i < net->n_layers; i++)
    {
        const nn_layer_t *L = &net->layers[i];
        const uint32_t n_in = (uint32_t)L->in_c * L->in_h * L->in_w;
        const uint32_t n_out = (uint32_t)L->out_c * L->out_h * L->out_w;
        if (n_in == 0u || n_out == 0u || n_in > net->max_act || n_out > net->max_act)
        {
            return false;
        }
        if (i > 0u)
        {
            const nn_layer_t *P = &net->layers[i - 1u];
            const uint32_t    n_prev = (uint32_t)P->out_c * P->out_h * P->out_w;
            /* A dense layer flattens, so only the count has to match. */
            const bool same = (L->kind == NN_DENSE)
                                  ? (n_prev == n_in)
                                  : (P->out_c == L->in_c && P->out_h == L->in_h && P->out_w == L->in_w);
            if (!same)
            {
                return false;
            }
        }
        switch (L->kind)
        {
        case NN_CONV:
        case NN_DWCONV:
            if (L->w == NULL || L->b == NULL || L->out_h != L->in_h || L->out_w != L->in_w ||
                (L->kind == NN_DWCONV && L->out_c != L->in_c))
            {
                return false;
            }
            break;
        case NN_POOL:
            if (L->out_c != L->in_c || L->out_h != L->in_h / 2u || L->out_w != L->in_w / 2u)
            {
                return false;
            }
            break;
        case NN_GAP:
            if (L->out_c != L->in_c || L->out_h != 1u || L->out_w != 1u)
            {
                return false;
            }
            break;
        case NN_DENSE:
            if (L->w == NULL || L->b == NULL || L->out_h != 1u || L->out_w != 1u)
            {
                return false;
            }
            break;
        default:
            return false;
        }
    }
    const nn_layer_t *last = &net->layers[net->n_layers - 1u];
    return last->out_c == 1u && last->out_h == 1u && last->out_w == 1u;
}

bool nn_forward(const nn_net_t *net, const float *patch, float *buf_a, float *buf_b, float *out)
{
    if (net == NULL || patch == NULL || buf_a == NULL || buf_b == NULL || out == NULL ||
        net->n_layers == 0u || net->layers == NULL || !shapes_ok(net))
    {
        if (out != NULL)
        {
            *out = 0.0f;
        }
        return false;
    }
    float *cur = buf_a, *nxt = buf_b;
    if (!lay_in(net, patch, cur))
    {
        *out = 0.0f;
        return false;
    }
    for (uint16_t i = 0u; i < net->n_layers; i++)
    {
        const nn_layer_t *L = &net->layers[i];
        switch (L->kind)
        {
        case NN_CONV:
            conv2d(L, cur, nxt, false);
            break;
        case NN_DWCONV:
            conv2d(L, cur, nxt, true);
            break;
        case NN_POOL:
            pool2(L, cur, nxt);
            break;
        case NN_GAP:
            gap(L, cur, nxt);
            break;
        default: /* NN_DENSE, validated above */
            dense(L, cur, nxt);
            break;
        }
        if (L->relu)
        {
            const uint32_t n = (uint32_t)L->out_c * L->out_h * L->out_w;
            for (uint32_t k = 0u; k < n; k++)
            {
                if (nxt[k] < 0.0f)
                {
                    nxt[k] = 0.0f;
                }
            }
        }
        float *t = cur;
        cur = nxt;
        nxt = t;
    }
    *out = cur[0];
    return true;
}
