/**
 ******************************************************************************
 * @file    tool_support.h
 * @brief   Argument and hex parsing shared by this package's host-side CLI test
 *          tools (codec_tool.c, linkframe_tool.c).
 *
 *          Shared rather than copied into each tool on purpose. parse_u32()
 *          in particular carries a history of subtle bugs (it silently
 *          accepted a wrapped negative, leading whitespace and a leading '+'
 *          before being hardened), and a second hand-copied version is exactly
 *          how one of them would come back: a fix applied to one copy and not
 *          the other. Same reasoning that made the codec read its bounds from
 *          the generated struct instead of a duplicated literal.
 ******************************************************************************
 */
#ifndef BOOMLINK_TOOL_SUPPORT_H
#define BOOMLINK_TOOL_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Whether this tool was built with the sanitizers, set by CMake from the
   BOOMLINK_SANITIZE option. Shared by both tools rather than repeated in each,
   because the -D on its own is worthless: what makes it mean anything is the
   report (`limits` prints sanitizers=, and conftest.py surfaces it) plus the
   cross-check below. A previous round shipped a tool that carried the -D and
   neither reported nor verified it, so the suite could claim a safety net it
   did not have.
   Defaults to 0, which UNDER-reports a hand-rolled build that passed
   -fsanitize=... without the -D. That direction is merely conservative; the
   dangerous one is claiming instrumentation that is absent, which would make a
   negative-path test's "clean rejection" verdict meaningless - so that
   direction is a compile error. GCC advertises ASan via __SANITIZE_ADDRESS__;
   Clang (verified on 18) does not define it at all and answers
   __has_feature(address_sanitizer) instead, so both are checked rather than
   exempting a whole compiler family. There is no equivalent macro for UBSan,
   which is why the value has to come from the build system in the first
   place. */
#ifndef BOOMLINK_SANITIZE_ENABLED
#define BOOMLINK_SANITIZE_ENABLED 0
#endif

#if defined(__SANITIZE_ADDRESS__)
#define BOOMLINK_ASAN_ACTIVE 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define BOOMLINK_ASAN_ACTIVE 1
#endif
#endif

#if BOOMLINK_SANITIZE_ENABLED && !defined(BOOMLINK_ASAN_ACTIVE)
#error "BOOMLINK_SANITIZE_ENABLED=1 but this tool is not compiled with -fsanitize=address"
#endif

/**
 * Parse `s` as a base-10 uint32_t. Rejects empty input, any leading character
 * that is not a digit (whitespace, '+', '-'), trailing junk, and anything out
 * of uint32_t range.
 *
 * `strtoul`/`strtoull` alone accept far more than that: leading whitespace
 * ("  5"), a leading '+' ("+5"), and - the sharpest edge - a leading '-' is
 * NOT rejected by the standard library. `strtoul("-1", ...)` parses the whole
 * string as a negated-then-wrapped unsigned value (ULONG_MAX) with `errno`
 * untouched, so a range check alone does not catch it either; an earlier
 * version of this function rejected "-1" only by accident, because wrapping
 * happened to land above 0xFFFFFFFF on a 64-bit `unsigned long`. The exact
 * same input would have been silently ACCEPTED where `unsigned long` is 32
 * bits (e.g. Windows LLP64). Requiring the first character to be '0'-'9'
 * rejects all of it in one portable check.
 *
 * Uses `strtoull`/`unsigned long long` (>= 64 bits per the C standard) rather
 * than `strtoul` for the same portability reason: the range check needs a type
 * strictly wider than uint32_t to mean anything.
 */
bool boomlink_tool_parse_u32(const char *s, uint32_t *out);

/**
 * Parse `s` as a base-10 uint8_t, with the same rules and rationale as
 * boomlink_tool_parse_u32().
 */
bool boomlink_tool_parse_u8(const char *s, uint8_t *out);

/**
 * Decode `hex` (even length, no separators) into `out` (capacity `out_cap`).
 * @return the decoded byte count, or -1 on malformed or oversized input. An
 *         empty string decodes to zero bytes.
 */
int boomlink_tool_hex_decode(const char *hex, uint8_t *out, size_t out_cap);

/**
 * Encode `len` bytes of `data` as lowercase hex into `out`, NUL-terminated.
 * `out_cap` must be at least 2 * len + 1; if it is not, this aborts rather
 * than truncating.
 *
 * abort(), not a silent truncation or a positive exit code: the test suite's
 * assert_clean_rejection() treats any positive exit status as a clean,
 * intentional rejection of the input, so an internal buffer-sizing fault that
 * exited positively would be indistinguishable from one - and measurably was,
 * once turning a real overflow-detecting test green. SIGABRT is the suite's
 * canonical internal-fault signal.
 */
void boomlink_tool_hex_encode(const uint8_t *data, size_t len, char *out, size_t out_cap);

#endif /* BOOMLINK_TOOL_SUPPORT_H */
