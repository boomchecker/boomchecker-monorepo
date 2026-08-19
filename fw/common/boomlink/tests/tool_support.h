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
