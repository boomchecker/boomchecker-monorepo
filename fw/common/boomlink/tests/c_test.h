/**
 ******************************************************************************
 * @file    c_test.h
 * @brief   A three-macro assertion helper for this package's C test binaries.
 *
 *          Not a framework and deliberately not one: the link engine's tests are
 *          stateful scenarios (send, advance the clock, expect a retry, advance
 *          again, expect failure), and what they need is a way to keep going
 *          after a failed expectation so one run reports every broken step
 *          rather than only the first.
 *
 *          Shared rather than copied into each test file for the same reason
 *          tool_support.h is: a second hand-maintained copy of "how a failure is
 *          reported" is how one file ends up silently not reporting them. That
 *          is not hypothetical in this package - a helper that exited positively
 *          instead of aborting once turned a real overflow test green.
 ******************************************************************************
 */
#ifndef BOOMLINK_C_TEST_H
#define BOOMLINK_C_TEST_H

#include <stdio.h>

/* Failure and check counters, defined once per test binary via
   BOOMLINK_TEST_STATE. */
extern int boomlink_test_failures;
extern int boomlink_test_checks;

#define BOOMLINK_TEST_STATE \
  int boomlink_test_failures = 0; \
  int boomlink_test_checks   = 0

/**
 * Expect `cond`. On failure, print where and why and carry on - see the header
 * comment for why this does not abort.
 *
 * The message is printf-style and mandatory, because "CHECK failed at line 214"
 * in a scenario that sends the same frame four times is not a diagnosis. Pass
 * the values under test, not a restatement of the condition.
 */
#define CHECK(cond, ...)                                                    \
  do {                                                                      \
    boomlink_test_checks++;                                                  \
    if (!(cond)) {                                                          \
      boomlink_test_failures++;                                              \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);                  \
      fprintf(stderr, __VA_ARGS__);                                         \
      fprintf(stderr, "\n  condition: %s\n", #cond);                        \
    }                                                                       \
  } while (0)

/**
 * Expect `cond`, and RETURN from the current scenario if it fails. For a
 * precondition whose failure would make every later expectation meaningless (a
 * NULL port, an engine that would not initialise) - continuing there produces a
 * page of consequential failures that bury the one that matters.
 */
#define REQUIRE(cond, ...)                                                  \
  do {                                                                      \
    boomlink_test_checks++;                                                  \
    if (!(cond)) {                                                          \
      boomlink_test_failures++;                                              \
      fprintf(stderr, "FAIL %s:%d (fatal to this scenario): ", __FILE__,    \
              __LINE__);                                                    \
      fprintf(stderr, __VA_ARGS__);                                         \
      fprintf(stderr, "\n  condition: %s\n", #cond);                        \
      return;                                                               \
    }                                                                       \
  } while (0)

/**
 * Report and exit. `min_checks` is a FLOOR on how many checks must have run.
 *
 * The floor exists because printing the count achieves nothing on its own, which
 * is what an earlier version of this comment wrongly claimed: ctest keys on the
 * exit code, hides stdout on success, and nothing compares the count to
 * anything. So a scenario hollowed out with an early `return` dropped
 * link_rx_test from 103 checks to 93 with ctest reporting 100% passed - verified.
 * Comparing against a floor here makes coverage disappearing a failure.
 *
 * Deliberately a floor rather than an exact count: adding checks must not require
 * touching a number, so the value only ever moves up, and the direction it fails
 * in is the one worth catching. Losing a whole scenario from main() is already a
 * compile error (-Werror=unused-function), so this covers the other half - a
 * scenario that still runs but stopped asserting.
 *
 * The `return 1` path below is the ONLY way any C test in this package can fail,
 * and it is exercised by tests/c_test_selfcheck.c via check_c_test.sh - without
 * that, changing it to `return 0` makes all four binaries pass forever while
 * printing their failures to stderr. That is not hypothetical: it was measured,
 * with two real duplicate-cache bugs green at the same time.
 */
#define BOOMLINK_TEST_REPORT(name, min_checks)                              \
  do {                                                                      \
    if (boomlink_test_checks < (min_checks)) {                              \
      fprintf(stderr,                                                       \
              "%s: only %d checks ran, expected at least %d - coverage "     \
              "disappeared rather than failing\n",                          \
              (name), boomlink_test_checks, (int)(min_checks));             \
      return 1;                                                             \
    }                                                                       \
    if (boomlink_test_failures != 0) {                                      \
      fprintf(stderr, "%s: %d of %d checks FAILED\n", (name),               \
              boomlink_test_failures, boomlink_test_checks);                \
      return 1;                                                             \
    }                                                                       \
    printf("%s: ok (%d checks)\n", (name), boomlink_test_checks);           \
    return 0;                                                               \
  } while (0)

#endif /* BOOMLINK_C_TEST_H */
