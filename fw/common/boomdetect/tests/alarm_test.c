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

int main(void)
{
    scenario_rule_validation();
    scenario_single_window_does_not_alarm();
    scenario_two_of_four_alarms_and_hysteresis_holds();
    scenario_hysteresis_prevents_flicker();
    scenario_full_width_history();
    BD_TEST_REPORT("alarm_test", 93); /* exact count from running the compiled binary */
}
