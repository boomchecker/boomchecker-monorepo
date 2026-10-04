/**
 * @file envelope.c
 * @brief The envelope ring; see envelope.h.
 *
 * Float32 throughout, like the rest of the chain on the board. The Python
 * specification filters in float64 and stores the 1 kHz envelope as float32;
 * a stable Butterworth section in float32 stays within about 1e-6 relative of
 * that, and the fixture (tests/vectors/extractor_mod_expected.h) states the
 * tolerance the features are then held to.
 */
#include "envelope.h"

#include "envelope_coefs.h"

#include <math.h>
#include <string.h>

_Static_assert(BOOMDETECT_ENV_SOS_SECTIONS == BOOMDETECT_ENV_SECTIONS,
               "envelope_coefs.h was generated with a different section count than "
               "boomdetect_t::env_state holds");
_Static_assert(BOOMDETECT_ENV_COEFS_DECIM == BOOMDETECT_ENV_DECIM &&
                   BOOMDETECT_ENV_COEFS_RING == BOOMDETECT_ENV_RING,
               "envelope_coefs.h and dsp_config.h disagree about the envelope geometry");
_Static_assert(BOOMDETECT_ENV_BANDPASS_SECTIONS < BOOMDETECT_ENV_SOS_SECTIONS,
               "the chain needs at least one low-pass section after the rectifier");

/* One section, direct form II transposed - the structure scipy.signal.sosfilt
   uses, so the rounding happens at the same places. c = b0 b1 b2 a1 a2. */
static inline float biquad(const float *c, float *z, float x)
{
    const float y = c[0] * x + z[0];
    z[0]          = c[1] * x - c[3] * y + z[1];
    z[1]          = c[2] * x - c[4] * y;
    return y;
}

void boomdetect_envelope_reset(boomdetect_t *d)
{
    memset(d->env_ring, 0, sizeof(d->env_ring));
    memset(d->env_state, 0, sizeof(d->env_state));
    d->env_head  = 0u;
    d->env_fill  = 0u;
    d->env_phase = 0u;
}

static void ring_push(boomdetect_t *d, float v)
{
    if (d->env_fill < BOOMDETECT_ENV_RING)
    {
        d->env_ring[(d->env_head + d->env_fill) % BOOMDETECT_ENV_RING] = v;
        d->env_fill++;
    }
    else
    {
        d->env_ring[d->env_head] = v;
        d->env_head              = (d->env_head + 1u) % BOOMDETECT_ENV_RING;
    }
}

void boomdetect_envelope_push(boomdetect_t *d, const float *x, uint32_t n)
{
    for (uint32_t i = 0u; i < n; i++)
    {
        float y = x[i];
        uint32_t s = 0u;
        for (; s < BOOMDETECT_ENV_BANDPASS_SECTIONS; s++)
        {
            y = biquad(boomdetect_env_sos[s], d->env_state[s], y);
        }
        y = fabsf(y);
        for (; s < BOOMDETECT_ENV_SOS_SECTIONS; s++)
        {
            y = biquad(boomdetect_env_sos[s], d->env_state[s], y);
        }
        /* e[::16]: the first sample of the stream is kept, then every 16th. */
        if (d->env_phase == 0u)
        {
            ring_push(d, y);
        }
        d->env_phase = (d->env_phase + 1u) % BOOMDETECT_ENV_DECIM;
    }
}

void boomdetect_envelope_gap(boomdetect_t *d)
{
    d->env_head = 0u;
    d->env_fill = 0u;
}

boomdetect_side_t boomdetect_envelope_side(const boomdetect_t *d)
{
    const boomdetect_side_t side = {
        .env      = d->env_ring,
        .env_head = d->env_head,
        .env_fill = d->env_fill,
    };
    return side;
}
