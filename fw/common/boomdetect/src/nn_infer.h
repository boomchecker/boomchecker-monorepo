/**
 * @file nn_infer.h
 * @brief A float32 interpreter for the small CNNs over the log-mel patch.
 *
 * Deliberately tiny: conv2d ('same' padding, stride 1, full or depthwise),
 * 2x2 max pool, global average pool and dense, which is every operation the
 * architectures in training/boomdetect_train/models/cnn.py use. No framework,
 * no quantisation, no dynamic allocation; a model is a table of layer
 * descriptors pointing at const weight arrays in a generated header, and the
 * caller lends two activation buffers of NN_MAX_ACT floats.
 *
 * Tensors are C x H x W, row-major, the layout PyTorch exports in. Weights are
 * (out_c, in_c_per_group, kh, kw) for convolutions and (out, in) for dense
 * layers, again as exported. cnn.py's forward_numpy() is the reference this
 * reproduces, and the parity fixture holds them to about 1e-5.
 */
#ifndef BOOMDETECT_NN_INFER_H
#define BOOMDETECT_NN_INFER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Layer kinds. Shared with cnn.py by value. */
#define NN_CONV   1u /* full conv2d */
#define NN_DWCONV 2u /* depthwise conv2d */
#define NN_POOL   3u /* 2x2 max pool, stride 2, floor */
#define NN_GAP    4u /* global average pool */
#define NN_DENSE  5u /* fully connected over the flattened activation */

/* How the 280-value patch (14 frames x 20 bands, frame-major) is laid in. */
#define NN_IN_FRAME_MAJOR  0u /* (1, 14, 20): in[0][t][m] = patch[t*20 + m] */
#define NN_IN_MEL_CHANNELS 1u /* (20, 1, 14): in[m][0][t] = patch[t*20 + m] */

typedef struct
{
    uint8_t  kind;
    uint8_t  relu;
    uint16_t in_c, in_h, in_w;
    uint16_t out_c, out_h, out_w;
    uint8_t  kh, kw, ph, pw;
    const float *w; /**< NULL for pool / gap */
    const float *b;
} nn_layer_t;

typedef struct
{
    uint8_t           input_layout;
    uint16_t          n_layers;
    const nn_layer_t *layers;
    uint32_t          max_act; /**< largest activation, in floats; buffers must hold it */
    uint32_t          patch_len;
} nn_net_t;

/**
 * @brief Run the network over one patch.
 * @param net   the model
 * @param patch nn_net_t::patch_len floats
 * @param buf_a scratch of net->max_act floats
 * @param buf_b scratch of net->max_act floats
 * @return the single output (a logit); 0 and false on a malformed descriptor
 */
bool nn_forward(const nn_net_t *net, const float *patch, float *buf_a, float *buf_b, float *out);

#ifdef __cplusplus
}
#endif

#endif /* BOOMDETECT_NN_INFER_H */
