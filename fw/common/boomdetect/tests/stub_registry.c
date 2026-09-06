/**
 * @file stub_registry.c
 * @brief A replacement registry with one fake model and no trained weights.
 *
 * classifier_registry.c's header claims it is a separate translation unit "so a
 * test can link its own registry with a stub model". That was true in principle
 * and untested in practice: every test linked the real archive, so if the
 * pipeline had ever grown a direct reference to the real table - an #include of
 * models.h, a call straight to classifier_mlp_v6.decide - the claim would have
 * quietly stopped holding and nothing would have said so.
 *
 * Linking this alongside the pipeline objects, without classifier_registry.c,
 * is the check. If it links and runs, the seam is real.
 */
#include "classifier.h"

#include <string.h>

/* Returns the feature count it was handed, which no real model does, so a test
   can tell beyond doubt which registry it is talking to. */
static float stub_decide(void *ctx, const float *features, uint16_t n)
{
    (void)ctx;
    (void)features;
    return (float)n;
}

static const classifier_t s_stub = {
    .name              = "stub",
    .layout_id         = BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX,
    .n_features        = 7u,
    .feature_offset    = 0u,
    .default_thr_milli = 1,
    .decide            = stub_decide,
    .ctx               = NULL,
};

static const classifier_t *const s_models[] = { &s_stub };

size_t classifier_count(void)
{
    return sizeof(s_models) / sizeof(s_models[0]);
}

const classifier_t *classifier_at(size_t idx)
{
    return (idx < classifier_count()) ? s_models[idx] : NULL;
}

const classifier_t *classifier_default(void)
{
    return s_models[0];
}

const classifier_t *classifier_by_name(const char *name)
{
    if (name == NULL)
    {
        return NULL;
    }
    return (strcmp(name, s_stub.name) == 0) ? &s_stub : NULL;
}
