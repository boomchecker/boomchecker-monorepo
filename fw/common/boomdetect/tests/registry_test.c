/**
 * @file registry_test.c
 * @brief The classifier registry: lookup, dispatch, and the checks that stop a
 *        mismatched model reading past the feature vector.
 */
#include "bd_test.h"
#include "boomdetect.h"
#include "classifier.h"

#include <math.h>
#include <string.h>

BD_TEST_STATE;

/* Returns the first feature it is given, so a test can prove exactly which
   slice of the vector reached decide(). */
static float probe_decide(void *ctx, const float *features, uint16_t n)
{
    (void)ctx;
    (void)n;
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
        CHECK((uint32_t)m->feature_offset + m->n_features <= BOOMDETECT_FEATURE_COUNT,
              "%s reads %u features from offset %u, past the %u the aggregate produces",
              m->name, m->n_features, m->feature_offset, (unsigned)BOOMDETECT_FEATURE_COUNT);
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
        .n_features = BOOMDETECT_FEATURE_COUNT, .feature_offset = 1u,
        .default_thr_milli = 0, .decide = probe_decide, .ctx = NULL,
    };
    cfg.classifier = &bad;
    CHECK(!boomdetect_init(&d, &cfg),
          "a model reading %u features from offset 1 fits in %u and should not",
          bad.n_features, (unsigned)BOOMDETECT_FEATURE_COUNT);

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

    /* NULL means "use the default", and proving that needs the entry that was
       adopted, not just a true return: init returning true is equally
       consistent with it having kept the previous model. */
    cfg.classifier = NULL;
    CHECK(boomdetect_init(&d, &cfg), "a NULL classifier should fall back to the default");
    CHECK(d.cfg.classifier == classifier_default(),
          "a NULL classifier resolved to '%s', not the default '%s'",
          (d.cfg.classifier != NULL) ? d.cfg.classifier->name : "(null)",
          classifier_default()->name);

    /* And the same for the threshold sentinel, which is the other thing a
       caller is entitled to leave to the model. */
    cfg.classifier = classifier_by_name("svm_v3");
    cfg.thr_milli  = BOOMDETECT_THR_MODEL_DEFAULT;
    REQUIRE(boomdetect_init(&d, &cfg), "init with the threshold sentinel failed");
    CHECK(d.cfg.thr_milli == classifier_by_name("svm_v3")->default_thr_milli,
          "the sentinel resolved to %ld, not svm_v3's %ld", (long)d.cfg.thr_milli,
          (long)classifier_by_name("svm_v3")->default_thr_milli);
    cfg.thr_milli = 0;

    cfg.classifier = &ok;
    cfg.decimation = 0u;
    CHECK(!boomdetect_init(&d, &cfg), "decimation 0 would divide by zero and was accepted");
}


/* The ~1700 trained floats are the reason this package exists, and until this
   scenario existed nothing on the host ever executed them: pipeline_test drives
   a stub and the checks above read metadata only. So the forward passes were
   never seen by ASan, and a NaN or an out-of-bounds weight index would have
   surfaced first on hardware. */
static void scenario_real_models_actually_run(void)
{
    static boomdetect_t d;
    boomdetect_event_t ev;
    static int16_t loud[BOOMDETECT_WINDOW_SIZE * 3u];

    /* A deterministic tone, the same one pipeline_test uses. Its absolute
       decision value is not asserted - that belongs to the parity fixture -
       only that each model runs, produces a finite number, and that the two
       families disagree, which is what proves both forward passes ran rather
       than one being silently reached twice. */
    static const int16_t lut[8] = { 0, 6000, 8000, 6000, 0, -6000, -8000, -6000 };
    for (uint32_t i = 0u; i < BOOMDETECT_WINDOW_SIZE * 3u; i++)
    {
        loud[i] = lut[i % 8u];
    }

    /* A second, clearly different signal. Deterministic integer noise, so the
       comparison below is reproducible on any platform. */
    static int16_t noisy[BOOMDETECT_WINDOW_SIZE * 3u];
    uint32_t seed = 1u;
    for (uint32_t i = 0u; i < BOOMDETECT_WINDOW_SIZE * 3u; i++)
    {
        seed = (seed * 1103515245u) + 12345u;
        noisy[i] = (int16_t)(((int32_t)((seed >> 16) & 0xFFFFu) - 32768) / 4);
    }

    float decisions[2][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
    for (size_t m = 0u; m < 2u; m++)
    {
        const classifier_t *model = classifier_at(m);
        REQUIRE(model != NULL, "classifier_at(%zu) is NULL", m);

        for (size_t sig = 0u; sig < 2u; sig++)
        {
            const int16_t *input = (sig == 0u) ? loud : noisy;
            boomdetect_config_t cfg = {
                .decimation = 3u,
                .squelch_milli = 0u,
                .thr_milli = model->default_thr_milli,
                .classifier = model,
            };
            REQUIRE(boomdetect_init(&d, &cfg), "init failed for %s", model->name);

            bool got = false;
            for (uint32_t f = 0u; f < BOOMDETECT_ACCUM_FRAMES + 4u && !got; f++)
            {
                boomdetect_push(&d, input, BOOMDETECT_HOP * 3u);
                while (boomdetect_step(&d, &ev))
                {
                    if (ev.window.complete)
                    {
                        decisions[m][sig] = ev.window.decision;
                        got = true;
                    }
                }
            }
            REQUIRE(got, "%s never completed a window on signal %zu", model->name, sig);
            CHECK(isfinite(decisions[m][sig]),
                  "%s produced a non-finite decision (%g) - a NaN here means a weight "
                  "table or a scaler is wrong, and nothing else would catch it",
                  model->name, (double)decisions[m][sig]);
        }

        /* The check that makes this scenario worth having. Without it a model
           whose forward pass was replaced by `return 0.0f` still passes: it is
           finite, and it still differs from the other family. A decision that
           does not move with the input is a model that is not reading it. */
        CHECK(decisions[m][0] != decisions[m][1],
              "%s returned the same decision (%.9g) for a tone and for noise, so its "
              "forward pass is not reading its input",
              model->name, (double)decisions[m][0]);
    }

    CHECK(decisions[0][0] != decisions[1][0],
          "mlp_v6 and svm_v3 returned the same decision (%.9g) on identical input; "
          "one forward pass is probably being dispatched twice",
          (double)decisions[0][0]);
}

int main(void)
{
    scenario_lookup();
    scenario_two_families_differ();
    scenario_init_rejects_bad_models();
    scenario_real_models_actually_run();
    BD_TEST_REPORT("registry_test", 48);  /* exact count from running the compiled binary */
}
