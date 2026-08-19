#!/bin/sh
# Verify boomlink_linkframe.h's encoder bound still works for C++ CALLERS.
#
# The encoder's output buffer is declared `uint8_t out[static 20]` in C, which
# makes an undersized caller buffer a compile error - including the intra-object
# case (a short array inside a larger struct) that AddressSanitizer is blind to,
# because the write stays inside a valid allocation. `[static N]` is not valid
# C++, so the header restores the same guarantee for C++ through a template
# wrapper plus an `extern "C"` declaration confined to a namespace. See the
# header for why the obvious plain overload does not work.
#
# That construct needs testing because it is delicate in a specific way: with
# both declarations at global scope the C function wins overload resolution
# outright and the template is never even instantiated, so a 4-byte buffer
# compiles clean. The failure mode is silence, not a build error.
#
# And it needs testing HERE because nothing else compiles this header as C++ at
# all: both libraries and both test tools are C. The C++ caller arrives with the
# firmware's radio layer in a later phase - which is exactly when a construct
# that had quietly stopped working would be discovered, on the target, by a
# corrupted neighbouring struct member.
#
# Both directions are checked, and the negative one is checked by REASON rather
# than by exit status alone: "the compiler exited nonzero" is also what a missing
# compiler, a wrong -I path or an unrelated typo produce, and a negative test
# that passes for those reasons is worse than no test.
#
# Usage: check_cxx_bound.sh <cxx-compiler> <std-flag> <include-dir> \
#                            <ok.cpp> <too-small.cpp>
set -eu

if [ "$#" -ne 5 ]; then
  echo "usage: $0 <cxx-compiler> <std-flag> <include-dir> <ok.cpp> <too-small.cpp>" >&2
  exit 2
fi

cxx=$1
std_flag=$2
include_dir=$3
ok_source=$4
too_small_source=$5

status=0

for source in "$ok_source" "$too_small_source"; do
  if [ ! -f "$source" ]; then
    echo "FAIL: $source does not exist - this check cannot verify anything" >&2
    exit 1
  fi
done

# A temporary output directory, so this test never writes into the source tree
# and two concurrent ctest runs cannot collide on an object file.
work=$(mktemp -d)
# shellcheck disable=SC2064 # $work must expand now, not when the trap fires.
trap "rm -rf '$work'" EXIT INT TERM

# 1. A correctly-sized buffer must compile, warning-free. -Werror matters here:
#    the wrapper takes the array by reference and calls through a namespaced
#    declaration, so a version of it that compiled but warned (an unused
#    parameter, a sign-compare in the static_assert's condition) would be a
#    smell worth failing on.
if ! ok_output=$("$cxx" "$std_flag" -Wall -Wextra -Werror -I"$include_dir" \
                   -c "$ok_source" -o "$work/ok.o" 2>&1); then
  echo "FAIL: a correctly-sized C++ caller no longer compiles:" >&2
  printf '%s\n' "$ok_output" >&2
  exit 1
fi
echo "ok: a correctly-sized C++ caller compiles clean"

# 2. The emitted call must be to the plain C symbol. If the template ever stopped
#    delegating - forwarding to itself, or to a C++-mangled overload - this TU
#    would still compile and still be bound-checked, but would not link against
#    the C implementation the firmware actually contains.
if ! symbols=$(nm "$work/ok.o" 2>&1); then
  echo "FAIL: cannot read symbols from the compiled C++ object: $symbols" >&2
  exit 1
fi
if ! printf '%s\n' "$symbols" | grep -qE ' U boomlink_linkframe_encode$'; then
  echo "FAIL: the C++ caller does not reference the unmangled C symbol" >&2
  echo "      boomlink_linkframe_encode; the template is not delegating to it." >&2
  printf '%s\n' "$symbols" >&2
  status=1
else
  echo "ok: the C++ caller references the unmangled C encoder symbol"
fi

# 3. An undersized buffer must NOT compile, and must be rejected by the header's
#    own static_assert - not by anything else that happens to fail.
if bad_output=$("$cxx" "$std_flag" -Wall -Wextra -I"$include_dir" \
                  -c "$too_small_source" -o "$work/bad.o" 2>&1); then
  echo "FAIL: an undersized output buffer compiled successfully from C++." >&2
  echo "      The encoder writes $(basename "$too_small_source")'s buffer past its end," >&2
  echo "      into the neighbouring struct member, where ASan cannot see it." >&2
  printf '%s\n' "$bad_output" >&2
  exit 1
fi
# The exact wording lives in boomlink_linkframe.h; matched loosely enough to
# survive a reworded message but tightly enough that an unrelated failure (no
# such file, unknown flag, a syntax error someone introduced) does not pass.
if ! printf '%s\n' "$bad_output" | grep -qi 'static assert'; then
  echo "FAIL: the undersized C++ caller was rejected, but NOT by the header's" >&2
  echo "      bound check - so this test would pass even with the bound removed:" >&2
  printf '%s\n' "$bad_output" >&2
  exit 1
fi
echo "ok: an undersized C++ output buffer is rejected by the header's bound check"

exit $status
