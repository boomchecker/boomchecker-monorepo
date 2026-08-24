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

/* Failure counter, defined once per test binary via BOOMLINK_TEST_MAIN_STATE. */
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
 * Report and exit. Prints the number of checks as well as the number of
 * failures, so a run that silently stopped exercising anything - a scenario list
 * that lost an entry, a file that compiled to nothing - is visible as a drop in
 * the count rather than as another clean pass.
 */
#define BOOMLINK_TEST_REPORT(name)                                          \
  do {                                                                      \
    if (boomlink_test_failures != 0) {                                      \
      fprintf(stderr, "%s: %d of %d checks FAILED\n", (name),               \
              boomlink_test_failures, boomlink_test_checks);                \
      return 1;                                                             \
    }                                                                       \
    printf("%s: ok (%d checks)\n", (name), boomlink_test_checks);           \
    return 0;                                                               \
  } while (0)

#endif /* BOOMLINK_C_TEST_H */
