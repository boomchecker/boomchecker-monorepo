/**
 * @file boomdetect_selftest.c
 * @brief Deterministic fixture generator; see boomdetect_selftest.h.
 */
#include "boomdetect_selftest.h"

#include <stdio.h>
#include <string.h>

/* One sample of the reference signal. Integer LCG, so every platform produces
   the same bits - a sinf() table would differ in the last place between a
   Cortex-M33 and an x86 host and would defeat the whole point. Amplitude is
   quartered to sit around -12 dBFS instead of slamming the CIC at full scale. */
static int16_t selftest_sample(uint32_t *state)
{
    *state = (*state * 1103515245u) + 12345u;
    int32_t v = (int32_t)((*state >> 16) & 0xFFFFu) - 32768; /* -32768..32767 */
    return (int16_t)(v / 4);
}

/* Raw bit pattern. Decimal would need %f (absent from newlib-nano) and would
   round away exactly the differences this exists to catch. */
static uint32_t selftest_bits(float v)
{
    uint32_t u;
    memcpy(&u, &v, sizeof(u));
    return u;
}

static void emit_vec(char *line, size_t len, const char *prefix, const float *v, uint32_t n,
                     boomdetect_selftest_emit_fn emit, void *ctx)
{
    int w = snprintf(line, len, "%s", prefix);
    for (uint32_t c = 0u; c < n && w > 0 && (size_t)w < len; c++)
    {
        w += snprintf(line + w, len - (size_t)w, " %08lX", (unsigned long)selftest_bits(v[c]));
    }
    emit(ctx, line);
}

bool boomdetect_selftest(boomdetect_t *d, const classifier_t *model, int16_t *block,
                         size_t block_len, boomdetect_selftest_emit_fn emit, void *ctx)
{
    static const char *stat[4] = { "mean", "std ", "dmea", "cmax" };
    char               line[160];
    char               prefix[24];

    if (emit == NULL)
    {
        return false;
    }
    if (d == NULL || model == NULL || block == NULL || block_len == 0u ||
        block_len > BOOMDETECT_SELFTEST_BLOCK || (block_len % 3u) != 0u)
    {
        emit(ctx, "DSTERR bad arguments");
        emit(ctx, "DSTEND frames=0 windows=0 err=1");
        return false;
    }

    /* No squelch: the gate is a policy knob, and letting it drop frames would
       make the fixture depend on the signal's level rather than on the
       arithmetic it is meant to pin down. The threshold comes from the model
       because a literal here would silently disagree with the registry; it does
       not reach the printed logit either way, only the DRONE/noise verdict the
       fixture does not record. */
    const boomdetect_config_t cfg = {
        .decimation    = 3u,
        .squelch_milli = 0u,
        .thr_milli     = model->default_thr_milli,
        .classifier    = model,
    };
    if (!boomdetect_init(d, &cfg))
    {
        emit(ctx, "DSTERR detector init failed");
        emit(ctx, "DSTEND frames=0 windows=0 err=1");
        return false;
    }

    emit(ctx, "DSTBEGIN");

    uint32_t state   = BOOMDETECT_SELFTEST_SEED;
    uint32_t fnv     = 2166136261u;
    uint32_t windows = 0u;

    for (uint32_t off = 0u; off < BOOMDETECT_SELFTEST_INPUT_LEN; off += (uint32_t)block_len)
    {
        /* Clamp rather than assume the block divides the input. It does not
           (66048 / 3072 = 21.5), and running the loop one block long generated
           1536 samples too many - which the fixture caught as a 43rd frame and
           a different checksum. A short final block is fine: boomdetect_push
           carries the decimation phase across calls. */
        const uint32_t left = BOOMDETECT_SELFTEST_INPUT_LEN - off;
        const uint32_t n    = (left < (uint32_t)block_len) ? left : (uint32_t)block_len;
        for (uint32_t i = 0u; i < n; i++)
        {
            const int16_t x = selftest_sample(&state);
            block[i]        = x;
            /* FNV-1a over the generated samples: proves both sides scored the
               same input before comparing anything downstream of it. */
            fnv = (fnv ^ (uint32_t)((uint16_t)x & 0xFFu)) * 16777619u;
            fnv = (fnv ^ (uint32_t)(((uint16_t)x >> 8) & 0xFFu)) * 16777619u;
        }
        boomdetect_push(d, block, n);

        /* Drain fully. The firmware's detect path caps this at one frame per
           block to keep USB fed; that is a pacing constraint, not arithmetic,
           so draining yields the same frames in the same order. */
        boomdetect_event_t ev;
        while (boomdetect_step(d, &ev))
        {
            const float *mf = boomdetect_last_mfcc(d);
            if (ev.frame_index < BOOMDETECT_SELFTEST_MFCC_FRAMES && mf != NULL)
            {
                snprintf(prefix, sizeof(prefix), "DSTMFCC f=%lu",
                         (unsigned long)ev.frame_index);
                emit_vec(line, sizeof(line), prefix, mf, BOOMDETECT_MFCC_COEFFS, emit, ctx);
            }

            if (ev.window.complete)
            {
                /* Every window, not just the first: the aggregate is rebuilt
                   from a reset accumulator each time, so a state bug shows up in
                   window 1 while window 0 still looks perfect. */
                const float *f = boomdetect_last_features(d);
                if (f == NULL)
                {
                    continue;
                }
                for (uint32_t g = 0u; g < 4u; g++)
                {
                    snprintf(prefix, sizeof(prefix), "DSTFEAT w=%lu %s",
                             (unsigned long)windows, stat[g]);
                    emit_vec(line, sizeof(line), prefix, f + g * BOOMDETECT_MFCC_COEFFS,
                             BOOMDETECT_MFCC_COEFFS, emit, ctx);
                }
                snprintf(line, sizeof(line), "DSTDEC w=%lu logit=%08lX",
                         (unsigned long)windows,
                         (unsigned long)selftest_bits(ev.window.decision));
                emit(ctx, line);
                windows++;
            }
        }
    }

    uint32_t win = 0u, dro = 0u;
    boomdetect_counts(d, &win, &dro);
    snprintf(line, sizeof(line), "DSTSIG n=%lu seed=%lu fnv=%08lX",
             (unsigned long)BOOMDETECT_SELFTEST_INPUT_LEN,
             (unsigned long)BOOMDETECT_SELFTEST_SEED, (unsigned long)fnv);
    emit(ctx, line);
    snprintf(line, sizeof(line), "DSTEND frames=%lu windows=%lu err=0",
             (unsigned long)boomdetect_frame_index(d), (unsigned long)win);
    emit(ctx, line);
    return true;
}
