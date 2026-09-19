/**
 * @file stub_registry_test.c
 * @brief Proves the registry is genuinely replaceable; see tests/stub_registry.c.
 */
#include "bd_test.h"
#include "boomdetect.h"
#include "classifier.h"

#include <string.h>

BD_TEST_STATE;

static void scenario_stub_registry_is_in_charge(void)
{
    REQUIRE(classifier_count() == 1u, "expected only the stub, got %zu models",
            classifier_count());
    const classifier_t *m = classifier_default();
    REQUIRE(m != NULL, "classifier_default() returned NULL");
    CHECK(strcmp(m->name, "stub") == 0,
          "the real registry linked in: default is '%s'", m->name);
    CHECK(classifier_by_name("mlp_v6") == NULL,
          "mlp_v6 resolved, so the real registry is present after all");
}

/* And the pipeline runs against it, which is the part that matters: the seam is
   worthless if boomdetect.c reaches the real table by some other route. */
static void scenario_pipeline_runs_on_the_stub(void)
{
    static boomdetect_t d;
    boomdetect_event_t  ev;
    static int16_t      buf[BOOMDETECT_WINDOW_SIZE * 3u];

    for (uint32_t i = 0u; i < BOOMDETECT_WINDOW_SIZE * 3u; i++)
    {
        buf[i] = (int16_t)((i % 8u) * 900u);
    }

    boomdetect_config_t cfg = {
        .decimation    = 3u,
        .squelch_milli = 0u,
        .thr_milli     = BOOMDETECT_THR_MODEL_DEFAULT,
        .classifier    = NULL, /* takes classifier_default(), i.e. the stub */
    };
    REQUIRE(boomdetect_init(&d, &cfg), "init failed against the stub registry");
    CHECK(d.cfg.classifier == classifier_default(),
          "init did not adopt the stub registry's default");

    bool got = false;
    for (uint32_t f = 0u; f < BOOMDETECT_ACCUM_FRAMES + 4u && !got; f++)
    {
        boomdetect_push(&d, buf, BOOMDETECT_HOP * 3u);
        while (boomdetect_step(&d, &ev))
        {
            if (ev.window.complete)
            {
                /* stub_decide returns its length argument, so this value can
                   only have come from the stub. */
                CHECK(ev.window.decision == 7.0f,
                      "decision %.9g is not the stub's n_features, so some other "
                      "decide() ran", (double)ev.window.decision);
                got = true;
            }
        }
    }
    CHECK(got, "no window completed against the stub registry");
}

int main(void)
{
    scenario_stub_registry_is_in_charge();
    scenario_pipeline_runs_on_the_stub();
    BD_TEST_REPORT("stub_registry_test", 8); /* exact count from running the binary */
}
