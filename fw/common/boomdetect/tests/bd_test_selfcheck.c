/**
 ******************************************************************************
 * @file    bd_test_selfcheck.c
 * @brief   A test binary that MUST fail. tests/check_bd_test.sh runs it and
 *          fails if it succeeds - or if it fails for the wrong reason.
 *
 *          bd_test.h's `return 1` is the only way any C test in this package can
 *          fail, and until this file existed nothing exercised it, because the
 *          path only runs when a test is already failing. Change it to
 *          `return 0` and all three C binaries here exit 0 forever while
 *          printing their failures to stderr, which ctest hides on success.
 *
 *          That is not hypothetical for this repository: the same harness in
 *          fw/common/boomlink was demonstrated failing exactly that way, with
 *          two real bugs green at the same time. This package copied the harness
 *          and, for a while, copied the claim that it was guarded without
 *          copying the guard.
 *
 *          Selected by BOOMDETECT_SELFCHECK_MODE so one binary covers each
 *          distinct path rather than four near-identical files.
 ******************************************************************************
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "bd_test.h"

BD_TEST_STATE;

/* One CHECK that fails and one that passes, so the failure counter has to be a
   counter rather than a boolean, and so a passing check after a failing one
   cannot clear it. */
static void scenario_with_a_failed_check(void)
{
    CHECK(1 == 1, "this one holds");
    CHECK(2 + 2 == 5, "deliberate failure: 2 + 2 is not %d", 5);
    CHECK(1 == 1, "and this one holds too, after the failure");
}

/* REQUIRE must both count the failure AND stop the scenario. If it returned
   without counting, a scenario could abort silently and the binary would still
   report success. */
static void scenario_with_a_failed_require(void)
{
    REQUIRE(0 == 1, "deliberate failure: a fatal precondition");
    /* Unreachable. If REQUIRE ever stops returning, this CHECK runs and the
       script sees the marker, which is how "REQUIRE did not stop the scenario"
       is told apart from "REQUIRE did not count". */
    CHECK(false, "REQUIRE-DID-NOT-RETURN");
}

/* Nothing fails, but far fewer checks run than the floor demands - the shape of
   a scenario hollowed out by an early return, which is otherwise invisible. */
static void scenario_that_barely_asserts(void)
{
    CHECK(1 == 1, "the only check in this mode");
}

int main(void)
{
    const char *mode = getenv("BOOMDETECT_SELFCHECK_MODE");
    if (mode == NULL)
    {
        fprintf(stderr, "bd_test_selfcheck: set BOOMDETECT_SELFCHECK_MODE to "
                        "check|require|floor|pass\n");
        return 2;
    }

    if (strcmp(mode, "check") == 0)
    {
        scenario_with_a_failed_check();
        BD_TEST_REPORT("selfcheck-check", 1);
    }
    if (strcmp(mode, "require") == 0)
    {
        scenario_with_a_failed_require();
        BD_TEST_REPORT("selfcheck-require", 1);
    }
    if (strcmp(mode, "floor") == 0)
    {
        scenario_that_barely_asserts();
        /* A floor no honest run can reach, standing in for coverage that
           vanished. */
        BD_TEST_REPORT("selfcheck-floor", 50);
    }
    if (strcmp(mode, "pass") == 0)
    {
        /* The control. Without it, a report macro that failed unconditionally
           would satisfy every case above - the same "passes for the wrong
           reason" trap the negative cases guard against, inverted. */
        scenario_with_a_failed_check();
        bd_test_failures = 0; /* pretend the failure never happened */
        BD_TEST_REPORT("selfcheck-pass", 1);
    }

    fprintf(stderr, "bd_test_selfcheck: unknown mode '%s'\n", mode);
    return 2;
}
