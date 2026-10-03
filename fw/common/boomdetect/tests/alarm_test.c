/**
 * @file alarm_test.c
 * @brief K-of-N with hysteresis. The scenarios repeat
 *        training/tests/test_decision.py, which is the specification.
 */
#include "bd_test.h"
#include "boomdetect_alarm.h"

BD_TEST_STATE;

static const boomdetect_alarm_rule_t two_of_four = { .n = 4u, .k_on = 2u, .k_off = 1u };

static void scenario_rule_validation(void)
{
    boomdetect_alarm_t a;
    CHECK(boomdetect_alarm_init(&a, &two_of_four), "2-of-4 (off below 1) is a valid rule");

    const boomdetect_alarm_rule_t k_on_past_n = { .n = 2u, .k_on = 3u, .k_off = 1u };
    CHECK(!boomdetect_alarm_init(&a, &k_on_past_n), "k_on > n was accepted");
    const boomdetect_alarm_rule_t k_off_past_k_on = { .n = 4u, .k_on = 1u, .k_off = 2u };
    CHECK(!boomdetect_alarm_init(&a, &k_off_past_k_on), "k_off > k_on was accepted");
    const boomdetect_alarm_rule_t zero_off = { .n = 4u, .k_on = 2u, .k_off = 0u };
    CHECK(!boomdetect_alarm_init(&a, &zero_off), "k_off 0 would never turn the alarm off");
    const boomdetect_alarm_rule_t too_long = { .n = 33u, .k_on = 2u, .k_off = 1u };
    CHECK(!boomdetect_alarm_init(&a, &too_long), "n past the history width was accepted");
    CHECK(!boomdetect_alarm_init(NULL, &two_of_four), "NULL alarm was accepted");
    CHECK(!boomdetect_alarm_init(&a, NULL), "NULL rule was accepted");
}

static void scenario_single_window_does_not_alarm(void)
{
    boomdetect_alarm_t a;
    REQUIRE(boomdetect_alarm_init(&a, &two_of_four), "init failed");
    const bool calls[8] = { 0, 1, 0, 0, 0, 1, 0, 0 };
    for (size_t i = 0u; i < 8u; i++)
    {
        CHECK(!boomdetect_alarm_push(&a, calls[i]), "state changed at window %u", (unsigned)i);
        CHECK(!boomdetect_alarm_on(&a), "a lone drone window raised the alarm at %u", (unsigned)i);
    }
    CHECK(a.onsets == 0u, "onsets %lu, expected 0", (unsigned long)a.onsets);
}

static void scenario_two_of_four_alarms_and_hysteresis_holds(void)
{
    boomdetect_alarm_t a;
    REQUIRE(boomdetect_alarm_init(&a, &two_of_four), "init failed");
    /*                     0  1  2  3  4  5  6  7  8 */
    const bool calls[9]  = { 0, 1, 1, 0, 0, 0, 0, 0, 0 };
    const bool expect[9] = { 0, 0, 1, 1, 1, 1, 0, 0, 0 };
    for (size_t i = 0u; i < 9u; i++)
    {
        const bool changed = boomdetect_alarm_push(&a, calls[i]);
        CHECK(boomdetect_alarm_on(&a) == expect[i], "window %u: state %d, expected %d",
              (unsigned)i, boomdetect_alarm_on(&a), expect[i]);
        CHECK(changed == (i == 2u || i == 6u), "window %u: changed=%d", (unsigned)i, changed);
    }
    CHECK(a.onsets == 1u, "onsets %lu, expected 1", (unsigned long)a.onsets);
    CHECK(boomdetect_alarm_hits(&a) == 0u, "hits %u after four quiet windows", boomdetect_alarm_hits(&a));
}

static void scenario_hysteresis_prevents_flicker(void)
{
    boomdetect_alarm_t a;
    REQUIRE(boomdetect_alarm_init(&a, &two_of_four), "init failed");
    const bool calls[12] = { 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0 };
    for (size_t i = 0u; i < 12u; i++)
    {
        (void)boomdetect_alarm_push(&a, calls[i]);
        if (i >= 1u && i <= 8u)
        {
            CHECK(boomdetect_alarm_on(&a), "alarm dropped at window %u while hits kept coming",
                  (unsigned)i);
        }
    }
    CHECK(!boomdetect_alarm_on(&a), "alarm still on after four quiet windows");
    CHECK(a.onsets == 1u, "onsets %lu, expected 1 (no flicker)", (unsigned long)a.onsets);
}

static void scenario_full_width_history(void)
{
    /* n = 32 uses every bit of the mask; k_on = 32 needs every window. */
    const boomdetect_alarm_rule_t all = { .n = 32u, .k_on = 32u, .k_off = 32u };
    boomdetect_alarm_t            a;
    REQUIRE(boomdetect_alarm_init(&a, &all), "init failed for n = 32");
    for (uint32_t i = 0u; i < 31u; i++)
    {
        (void)boomdetect_alarm_push(&a, true);
        CHECK(!boomdetect_alarm_on(&a), "alarm on after %lu of 32 hits", (unsigned long)(i + 1u));
    }
    CHECK(boomdetect_alarm_push(&a, true), "32nd hit did not change the state");
    CHECK(boomdetect_alarm_on(&a), "alarm off with 32 of 32 hits");
    CHECK(boomdetect_alarm_hits(&a) == 32u, "hits %u, expected 32", boomdetect_alarm_hits(&a));
    (void)boomdetect_alarm_push(&a, false);
    CHECK(!boomdetect_alarm_on(&a), "alarm on with 31 of 32 hits under k_off 32");
}

static void scenario_vote_reads_the_sign_of_a_decision(void)
{
    /* push_decision under VOTE is the two-of-four scenario with the calls
       given as relative decisions: >= 0 is a hit, including exactly 0. */
    boomdetect_alarm_t a;
    REQUIRE(boomdetect_alarm_init(&a, &two_of_four), "init failed");
    const float rel[9]   = { -1.0f, 0.0f, 0.3f, -0.1f, -2.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    const bool expect[9] = { 0, 0, 1, 1, 1, 1, 0, 0, 0 };
    for (size_t i = 0u; i < 9u; i++)
    {
        const bool changed = boomdetect_alarm_push_decision(&a, rel[i]);
        CHECK(boomdetect_alarm_on(&a) == expect[i], "window %u: state %d, expected %d",
              (unsigned)i, boomdetect_alarm_on(&a), expect[i]);
        CHECK(changed == (i == 2u || i == 6u), "window %u: changed=%d", (unsigned)i, changed);
    }
    CHECK(a.onsets == 1u, "onsets %lu, expected 1", (unsigned long)a.onsets);
}

static void scenario_mean_rule_integrates(void)
{
    /* mean4: the mean of the last four relative decisions, windows before the
       first counting as 0. Mirrors test_mean_rule_integrates in decision.py. */
    const boomdetect_alarm_rule_t mean4 = { .n = 4u, .mode = BOOMDETECT_ALARM_MEAN };
    boomdetect_alarm_t            a;
    REQUIRE(boomdetect_alarm_init(&a, &mean4), "init failed for mean4");
    CHECK(boomdetect_alarm_mean(&a) == 0.0f, "mean before any window is %f", (double)boomdetect_alarm_mean(&a));

    /*                   0      1     2     3      4      5      6  */
    const float rel[7]   = { -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    const float means[7] = { -0.25f, 0.0f, 0.25f, 0.0f, 0.0f, -0.5f, -1.0f };
    const bool expect[7] = { 0, 1, 1, 1, 1, 0, 0 };
    for (size_t i = 0u; i < 7u; i++)
    {
        const bool changed = boomdetect_alarm_push_decision(&a, rel[i]);
        const float m      = boomdetect_alarm_mean(&a);
        CHECK(m > means[i] - 1e-6f && m < means[i] + 1e-6f, "window %u: mean %f, expected %f",
              (unsigned)i, (double)m, (double)means[i]);
        CHECK(boomdetect_alarm_on(&a) == expect[i], "window %u: state %d, expected %d",
              (unsigned)i, boomdetect_alarm_on(&a), expect[i]);
        CHECK(changed == (i == 1u || i == 5u), "window %u: changed=%d", (unsigned)i, changed);
    }
    CHECK(a.onsets == 1u, "onsets %lu, expected 1", (unsigned long)a.onsets);
    CHECK(boomdetect_alarm_hits(&a) == 0u, "hits %u with four negative decisions",
          boomdetect_alarm_hits(&a));

    /* One window well above the threshold carries three weak ones under MEAN;
       the same four windows give the vote rule nothing to count. */
    REQUIRE(boomdetect_alarm_init(&a, &mean4), "re-init failed");
    const float weak[4] = { 3.0f, -0.5f, -0.5f, -0.5f };
    for (size_t i = 0u; i < 4u; i++)
    {
        (void)boomdetect_alarm_push_decision(&a, weak[i]);
        CHECK(boomdetect_alarm_on(&a), "mean4 dropped at window %u (mean %f)", (unsigned)i,
              (double)boomdetect_alarm_mean(&a));
    }
    boomdetect_alarm_t v;
    REQUIRE(boomdetect_alarm_init(&v, &two_of_four), "init failed");
    for (size_t i = 0u; i < 4u; i++)
    {
        (void)boomdetect_alarm_push_decision(&v, weak[i]);
    }
    CHECK(!boomdetect_alarm_on(&v), "2of4 alarmed on one strong window");

    /* push(bool) under MEAN enters +1 / -1. */
    REQUIRE(boomdetect_alarm_init(&a, &mean4), "re-init failed");
    (void)boomdetect_alarm_push(&a, true);
    CHECK(boomdetect_alarm_mean(&a) == 0.25f, "push(true) did not enter +1 (mean %f)",
          (double)boomdetect_alarm_mean(&a));
    CHECK(boomdetect_alarm_on(&a), "mean4 off after one drone call from silence");
}

static void scenario_mean_rule_validation(void)
{
    boomdetect_alarm_t a;
    const boomdetect_alarm_rule_t mean_ignores_k = { .n = 8u, .k_on = 0u, .k_off = 0u,
                                                     .mode = BOOMDETECT_ALARM_MEAN };
    CHECK(boomdetect_alarm_init(&a, &mean_ignores_k), "MEAN rejected a rule with k_on/k_off 0");
    const boomdetect_alarm_rule_t mean_zero = { .n = 0u, .mode = BOOMDETECT_ALARM_MEAN };
    CHECK(!boomdetect_alarm_init(&a, &mean_zero), "MEAN with n 0 was accepted");
    const boomdetect_alarm_rule_t mean_long = { .n = 33u, .mode = BOOMDETECT_ALARM_MEAN };
    CHECK(!boomdetect_alarm_init(&a, &mean_long), "MEAN with n 33 was accepted");
    const boomdetect_alarm_rule_t unknown = { .n = 4u, .k_on = 2u, .k_off = 1u, .mode = 2u };
    CHECK(!boomdetect_alarm_init(&a, &unknown), "an unknown mode was accepted");
}

int main(void)
{
    scenario_rule_validation();
    scenario_single_window_does_not_alarm();
    scenario_two_of_four_alarms_and_hysteresis_holds();
    scenario_hysteresis_prevents_flicker();
    scenario_full_width_history();
    scenario_vote_reads_the_sign_of_a_decision();
    scenario_mean_rule_integrates();
    scenario_mean_rule_validation();
    BD_TEST_REPORT("alarm_test", 152); /* exact count from running the compiled binary */
}
