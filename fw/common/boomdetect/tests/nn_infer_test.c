/**
 * @file nn_infer_test.c
 * @brief The CNN interpreter on networks small enough to work out by hand.
 *
 * The real models are checked against their Python forward pass by
 * model_parity_test; this pins the indexing conventions (row-major C x H x W,
 * (out, in, kh, kw) weights, 'same' padding, floor pooling, the mel-channel
 * input transpose) on inputs where a wrong index gives a visibly wrong number.
 */
#include "bd_test.h"
#include "nn_infer.h"

#include <math.h>
#include <string.h>

BD_TEST_STATE;

#define PATCH_LEN 280u
static float s_patch[PATCH_LEN];
static float s_a[PATCH_LEN * 4u];
static float s_b[PATCH_LEN * 4u];

static void fill_patch(void)
{
    for (uint32_t i = 0u; i < PATCH_LEN; i++)
    {
        s_patch[i] = (float)i; /* patch[t*20 + m] = t*20 + m */
    }
}

/* conv 1->1, 3x3 with only the centre weight set, then GAP: the network is the
   mean of the patch times the weight plus the bias. */
static void scenario_identity_conv_then_gap(void)
{
    static const float w[9] = { 0, 0, 0, 0, 2.0f, 0, 0, 0, 0 };
    static const float b[1] = { 0.5f };
    static const float wd[1] = { 1.0f };
    static const float bd[1] = { 0.0f };
    const nn_layer_t layers[3] = {
        { NN_CONV, 0, 1, 14, 20, 1, 14, 20, 3, 3, 1, 1, w, b },
        { NN_GAP, 0, 1, 14, 20, 1, 1, 1, 0, 0, 0, 0, NULL, NULL },
        { NN_DENSE, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, wd, bd },
    };
    const nn_net_t net = { NN_IN_FRAME_MAJOR, 3u, layers, PATCH_LEN, PATCH_LEN };
    fill_patch();
    float out = -1.0f;
    REQUIRE(nn_forward(&net, s_patch, s_a, s_b, &out), "forward failed");
    const float want = 2.0f * 139.5f + 0.5f; /* mean of 0..279 is 139.5 */
    CHECK(fabsf(out - want) < 1e-3f, "got %.6f, expected %.6f", (double)out, (double)want);
}

/* A 3x3 kernel with weight only at the top-left tap reads in[y-1][x-1]: with the
   patch as values, out[0][y][x] = patch[(y-1)*20 + (x-1)] inside, 0 on the
   padded border. Check one interior cell and one border cell through a dense
   layer that selects them. */
static void scenario_padding_and_tap_orientation(void)
{
    static const float w[9] = { 1.0f, 0, 0, 0, 0, 0, 0, 0, 0 };
    static const float b[1] = { 0.0f };
    static float wd[280];
    static const float bd[1] = { 0.0f };
    const nn_layer_t layers[2] = {
        { NN_CONV, 0, 1, 14, 20, 1, 14, 20, 3, 3, 1, 1, w, b },
        { NN_DENSE, 0, 1, 14, 20, 1, 1, 1, 0, 0, 0, 0, wd, bd },
    };
    const nn_net_t net = { NN_IN_FRAME_MAJOR, 2u, layers, PATCH_LEN, PATCH_LEN };
    fill_patch();

    memset(wd, 0, sizeof(wd));
    wd[5 * 20 + 7] = 1.0f; /* select out[5][7] = patch[4*20 + 6] = 86 */
    float out = 0.0f;
    REQUIRE(nn_forward(&net, s_patch, s_a, s_b, &out), "forward failed");
    CHECK(out == 86.0f, "interior tap read %.1f, expected 86", (double)out);

    memset(wd, 0, sizeof(wd));
    wd[0] = 1.0f; /* out[0][0] reads the padded corner: 0 */
    REQUIRE(nn_forward(&net, s_patch, s_a, s_b, &out), "forward failed");
    CHECK(out == 0.0f, "padded corner read %.1f, expected 0", (double)out);
}

/* Depthwise: two channels, each with its own centre weight; GAP; dense sums
   them. Channel c of the input is a constant c+1. */
static void scenario_depthwise_keeps_channels_apart(void)
{
    static float in_patch[2 * 3 * 4];
    static const float w[2 * 9] = { 0, 0, 0, 0, 10.0f, 0, 0, 0, 0, 0, 0, 0, 0, 100.0f, 0, 0, 0, 0 };
    static const float b[2] = { 0.0f, 0.0f };
    static const float wd[2] = { 1.0f, 1.0f };
    static const float bd[1] = { 0.0f };
    const nn_layer_t layers[3] = {
        { NN_DWCONV, 0, 2, 3, 4, 2, 3, 4, 3, 3, 1, 1, w, b },
        { NN_GAP, 0, 2, 3, 4, 2, 1, 1, 0, 0, 0, 0, NULL, NULL },
        { NN_DENSE, 0, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, wd, bd },
    };
    const nn_net_t net = { NN_IN_FRAME_MAJOR, 3u, layers, 24u, 24u };
    for (uint32_t i = 0u; i < 24u; i++)
    {
        in_patch[i] = (i < 12u) ? 1.0f : 2.0f;
    }
    float out = 0.0f;
    REQUIRE(nn_forward(&net, in_patch, s_a, s_b, &out), "forward failed");
    CHECK(out == 10.0f * 1.0f + 100.0f * 2.0f, "got %.1f, expected 210", (double)out);
}

/* 2x2 max pool with floor: a 3x5 image pools to 1x2, dropping the last row and
   column. Values increase along the row, so each pooled cell is its right-bottom
   element of the kept block. */
static void scenario_pool_floors_and_takes_max(void)
{
    static float img[15];
    static const float wd[2] = { 1.0f, 10.0f };
    static const float bd[1] = { 0.0f };
    const nn_layer_t layers[2] = {
        { NN_POOL, 0, 1, 3, 5, 1, 1, 2, 2, 2, 0, 0, NULL, NULL },
        { NN_DENSE, 0, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, wd, bd },
    };
    const nn_net_t net = { NN_IN_FRAME_MAJOR, 2u, layers, 15u, 15u };
    for (uint32_t i = 0u; i < 15u; i++)
    {
        img[i] = (float)i;
    }
    float out = 0.0f;
    REQUIRE(nn_forward(&net, img, s_a, s_b, &out), "forward failed");
    /* block (rows 0-1, cols 0-1) max = 6; block (rows 0-1, cols 2-3) max = 8 */
    CHECK(out == 6.0f + 10.0f * 8.0f, "got %.1f, expected 86", (double)out);
}

/* The mel-channel input layout: in[m][0][t] = patch[t*20 + m]. A 1x1 conv over
   20 channels that selects channel 3 and a dense layer that selects t = 7 must
   read patch[7*20 + 3] = 143. */
static void scenario_mel_channel_input_transpose(void)
{
    static float w[20];
    static const float b[1] = { 0.0f };
    static float wd[14];
    static const float bd[1] = { 0.0f };
    const nn_layer_t layers[2] = {
        { NN_CONV, 0, 20, 1, 14, 1, 1, 14, 1, 1, 0, 0, w, b },
        { NN_DENSE, 0, 1, 1, 14, 1, 1, 1, 0, 0, 0, 0, wd, bd },
    };
    const nn_net_t net = { NN_IN_MEL_CHANNELS, 2u, layers, PATCH_LEN, PATCH_LEN };
    fill_patch();
    memset(w, 0, sizeof(w));
    w[3] = 1.0f;
    memset(wd, 0, sizeof(wd));
    wd[7] = 1.0f;
    float out = 0.0f;
    REQUIRE(nn_forward(&net, s_patch, s_a, s_b, &out), "forward failed");
    CHECK(out == 143.0f, "got %.1f, expected 143", (double)out);
}

static void scenario_relu_and_rejections(void)
{
    static const float wd[1] = { 1.0f };
    static const float bd[1] = { -5.0f };
    const nn_layer_t relu_then_out[2] = {
        { NN_DENSE, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, wd, bd },
        { NN_DENSE, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, wd, bd },
    };
    const nn_net_t net = { NN_IN_FRAME_MAJOR, 2u, relu_then_out, 1u, 1u };
    float in = 2.0f, out = 1.0f;
    REQUIRE(nn_forward(&net, &in, s_a, s_b, &out), "forward failed");
    CHECK(out == -5.0f, "ReLU did not clamp: got %.1f, expected -5 (0 - 5)", (double)out);

    /* A descriptor whose shapes do not chain must be refused, not read. */
    const nn_layer_t broken[2] = {
        { NN_DENSE, 0, 1, 1, 1, 3, 1, 1, 0, 0, 0, 0, wd, bd },
        { NN_DENSE, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, wd, bd },
    };
    const nn_net_t bad = { NN_IN_FRAME_MAJOR, 2u, broken, 3u, 1u };
    out = 7.0f;
    CHECK(!nn_forward(&bad, &in, s_a, s_b, &out), "mismatched layer shapes were accepted");
    CHECK(out == 0.0f, "a rejected forward left out at %.1f", (double)out);
    CHECK(!nn_forward(&net, NULL, s_a, s_b, &out), "NULL patch was accepted");
    CHECK(!nn_forward(NULL, &in, s_a, s_b, &out), "NULL net was accepted");
}

int main(void)
{
    scenario_identity_conv_then_gap();
    scenario_padding_and_tap_orientation();
    scenario_depthwise_keeps_channels_apart();
    scenario_pool_floors_and_takes_max();
    scenario_mel_channel_input_transpose();
    scenario_relu_and_rejections();
    BD_TEST_REPORT("nn_infer_test", 18); /* exact count from running the compiled binary */
}
