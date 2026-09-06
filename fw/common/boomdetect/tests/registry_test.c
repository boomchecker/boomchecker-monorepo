/**
 * @file registry_test.c
 * @brief The classifier registry: lookup, dispatch, and the checks that stop a
 *        mismatched model reading past the feature vector.
 */
#include "bd_test.h"
#include "boomdetect.h"
#include "classifier.h"

#include <string.h>

BD_TEST_STATE;

/* Returns the first feature it is given, so a test can prove exactly which
   slice of the vector reached decide(). */
static float probe_decide(const void *ctx, const float *features)
{
    (void)ctx;
    return features[0];
}

static void scenario_lookup(void)
{
    REQUIRE(classifier_count() >= 2u, "expected at least two models, got %zu",
            classifier_count());

    const classifier_t *def = classifier_default();
    REQUIRE(def != NULL, "classifier_default() returned NULL");
    CHECK(strcmp(def->name, "mlp_v6") == 0,
          "the deployed model should be the default, got '%s'", def->name);

    CHECK(classifier_by_name("mlp_v6") == def,
          "by_name(\"mlp_v6\") did not return the same entry as default()");
    CHECK(classifier_by_name("svm_v3") != NULL, "svm_v3 is missing from the registry");
    CHECK(classifier_by_name("nope") == NULL, "an unknown name resolved to something");
    CHECK(classifier_by_name(NULL) == NULL, "a NULL name did not resolve to NULL");

    CHECK(classifier_at(classifier_count()) == NULL,
          "classifier_at() past the end should be NULL");

    /* Every entry has to be usable, not just the two named above. */
    for (size_t i = 0u; i < classifier_count(); i++)
    {
        const classifier_t *m = classifier_at(i);
        REQUIRE(m != NULL, "classifier_at(%zu) is NULL below the count", i);
        CHECK(m->decide != NULL, "%s has no decide function", m->name);
        CHECK(m->n_features > 0u, "%s reads zero features", m->name);
        CHECK((uint32_t)m->feature_offset + m->n_features <= DET_FEATURE_COUNT,
              "%s reads %u features from offset %u, past the %u the aggregate produces",
              m->name, m->n_features, m->feature_offset, (unsigned)DET_FEATURE_COUNT);
        CHECK(m->layout_id == BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX,
              "%s declares layout %u, this build produces %u", m->name, m->layout_id,
              BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX);
    }
}

/* The two families exist side by side precisely because they read different
   slices; if they did not, the offset would never be exercised. */
static void scenario_two_families_differ(void)
{
    const classifier_t *mlp = classifier_by_name("mlp_v6");
    const classifier_t *svm = classifier_by_name("svm_v3");
    REQUIRE(mlp != NULL && svm != NULL, "both models must be present");

    CHECK(mlp->feature_offset != svm->feature_offset,
          "both models start at feature %u, so dispatch through feature_offset is "
          "never actually tested", mlp->feature_offset);
    CHECK(mlp->n_features != svm->n_features,
          "both models read %u features, so dispatch through n_features is never "
          "actually tested", mlp->n_features);
    CHECK(mlp->default_thr_milli != svm->default_thr_milli,
          "both models share a threshold of %ld, which would hide the whole reason "
          "the operating point belongs to the model",
          (long)mlp->default_thr_milli);
}

/* boomdetect_init has to refuse a model that would read past the vector, since
   a caller can hand it any entry it likes - the per-model _Static_asserts only
   cover the ones compiled in. */
static void scenario_init_rejects_bad_models(void)
{
    static boomdetect_t d;
    boomdetect_config_t cfg = { .decimation = 3u, .squelch_milli = 0u, .thr_milli = 0 };

    classifier_t bad = {
        .name = "bad", .layout_id = BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX,
        .n_features = DET_FEATURE_COUNT, .feature_offset = 1u,
        .default_thr_milli = 0, .decide = probe_decide, .ctx = NULL,
    };
    cfg.classifier = &bad;
    CHECK(!boomdetect_init(&d, &cfg),
          "a model reading %u features from offset 1 fits in %u and should not",
          bad.n_features, (unsigned)DET_FEATURE_COUNT);

    classifier_t wrong_layout = bad;
    wrong_layout.n_features = 4u;
    wrong_layout.layout_id = BOOMDETECT_LAYOUT_MEAN_STD_DMEAN_CMAX + 1u;
    cfg.classifier = &wrong_layout;
    CHECK(!boomdetect_init(&d, &cfg), "a model declaring a foreign layout was accepted");

    classifier_t no_decide = bad;
    no_decide.n_features = 4u;
    no_decide.decide = NULL;
    cfg.classifier = &no_decide;
    CHECK(!boomdetect_init(&d, &cfg), "a model with no decide function was accepted");

    classifier_t ok = bad;
    ok.n_features = 4u;
    cfg.classifier = &ok;
    CHECK(boomdetect_init(&d, &cfg), "a model that fits was rejected");

    cfg.classifier = NULL;
    CHECK(boomdetect_init(&d, &cfg), "a NULL classifier should fall back to the default");

    cfg.classifier = &ok;
    cfg.decimation = 0u;
    CHECK(!boomdetect_init(&d, &cfg), "decimation 0 would divide by zero and was accepted");
}

int main(void)
{
    scenario_lookup();
    scenario_two_families_differ();
    scenario_init_rejects_bad_models();
    BD_TEST_REPORT("registry_test", 20);
}
