#!/bin/sh
# Verify that this package's C test harness can actually FAIL.
#
# c_test.h's `return 1` is the only way any C test here can fail, and nothing
# exercised it, because it only runs when a test is already failing. Changing it
# to `return 0` made every C binary built from c_test.h - every check in every
# one of them, a count this comment doesn't restate since it only grows - exit 0
# forever while printing its failures to stderr; demonstrated with two real
# duplicate-cache bugs green at the same time. One character, the entire C suite.
#
# Four modes, each a distinct way the harness could be broken:
#
#   check    a failed CHECK must make the process exit nonzero
#   require  a failed REQUIRE must count AND stop its scenario
#   floor    too few checks must fail, even with nothing failing - this is what
#            makes the check count load-bearing instead of decorative
#   pass     a clean run must still exit 0, so the three above cannot be
#            satisfied by a report macro that fails unconditionally
#
# Every negative case is checked by REASON as well as by exit status: "exited
# nonzero" is also what a crash, a missing binary or an unknown mode produce, and
# a negative test that passes for those reasons is worse than no test. The same
# discipline as check_encoder_bound.sh and check_no_nanopb.sh.
#
# Usage: check_c_test.sh <selfcheck-binary>
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <selfcheck-binary>" >&2
  exit 2
fi
binary=$1

if [ ! -x "$binary" ]; then
  echo "FAIL: $binary is not executable - nothing was verified" >&2
  exit 1
fi

status=0

# $1 mode, $2 expected exit status, $3 a regex the output must contain
expect() {
  mode=$1
  want_rc=$2
  want_text=$3
  # The command runs as its own statement, not inside an `if`, so a crash is
  # distinguishable from a controlled failure: `set -e` is suspended inside an
  # `if` condition, which is how an earlier check in this package failed open.
  out=$(BOOMLINK_SELFCHECK_MODE="$mode" "$binary" 2>&1) && rc=0 || rc=$?

  if [ "$rc" -ne "$want_rc" ]; then
    echo "FAIL: mode '$mode' exited $rc, expected $want_rc" >&2
    printf '%s\n' "$out" >&2
    status=1
    return
  fi
  if ! printf '%s\n' "$out" | grep -qE "$want_text"; then
    echo "FAIL: mode '$mode' exited $rc as expected, but for the wrong reason -" >&2
    echo "      its output does not match /$want_text/, so this case would pass" >&2
    echo "      even with the harness broken:" >&2
    printf '%s\n' "$out" >&2
    status=1
    return
  fi
  echo "ok: $mode"
}

# A failed CHECK must fail the process, and the report must name how many of how
# many failed - the count is what tells a reader whether one thing broke or
# everything did.
expect check 1 '1 of 3 checks FAILED'

# A failed REQUIRE must count its failure and stop the scenario. The marker
# string only appears if REQUIRE stopped returning, which separates "did not
# count" from "did not return".
expect require 1 '1 of 1 checks FAILED'
if BOOMLINK_SELFCHECK_MODE=require "$binary" 2>&1 | grep -q 'REQUIRE-DID-NOT-RETURN'; then
  echo "FAIL: REQUIRE did not stop its scenario - a scenario could abort silently" >&2
  status=1
fi

# Too few checks must fail even though nothing failed.
expect floor 1 'only 1 checks ran, expected at least 50'

# And a clean run must still pass, or every case above is satisfied by a report
# macro that always fails.
expect pass 0 'selfcheck-pass: ok'

exit $status
