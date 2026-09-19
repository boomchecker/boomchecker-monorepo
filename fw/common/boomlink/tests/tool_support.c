/**
 ******************************************************************************
 * @file    tool_support.c
 ******************************************************************************
 */
#include "tool_support.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* See the header for why the first-character check exists and why this uses
   strtoull rather than strtoul. */
static bool parse_ull_bounded(const char *s, unsigned long long limit, unsigned long long *out) {
  if (s == NULL || s[0] < '0' || s[0] > '9') {
    return false;
  }
  errno                    = 0;
  char              *end   = NULL;
  unsigned long long value = strtoull(s, &end, 10);
  if (*end != '\0' || errno == ERANGE || value > limit) {
    return false;
  }
  *out = value;
  return true;
}

bool boomlink_tool_parse_u32(const char *s, uint32_t *out) {
  unsigned long long value = 0;
  if (!parse_ull_bounded(s, 0xFFFFFFFFULL, &value)) {
    return false;
  }
  *out = (uint32_t)value;
  return true;
}

bool boomlink_tool_parse_u8(const char *s, uint8_t *out) {
  unsigned long long value = 0;
  if (!parse_ull_bounded(s, 0xFFULL, &value)) {
    return false;
  }
  *out = (uint8_t)value;
  return true;
}

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

int boomlink_tool_hex_decode(const char *hex, uint8_t *out, size_t out_cap) {
  size_t hex_len = strlen(hex);
  if (hex_len % 2 != 0) return -1;
  size_t out_len = hex_len / 2;
  if (out_len > out_cap) return -1;
  for (size_t i = 0; i < out_len; i++) {
    int hi = hex_nibble(hex[2 * i]);
    int lo = hex_nibble(hex[2 * i + 1]);
    if (hi < 0 || lo < 0) return -1;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return (int)out_len;
}

void boomlink_tool_hex_encode(const uint8_t *data, size_t len, char *out, size_t out_cap) {
  static const char digits[] = "0123456789abcdef";
  if (out_cap < 2 * len + 1) {
    /* See the header for why this aborts instead of truncating or exiting with
       a positive status. */
    fprintf(stderr,
            "hex_encode: output buffer too small for a %zu-byte payload (capacity %zu) - "
            "this is a bug in the tool's buffer sizing, not a bad input\n",
            len, out_cap);
    fflush(stderr);
    abort();
  }
  for (size_t i = 0; i < len; i++) {
    out[2 * i]     = digits[(data[i] >> 4) & 0xF];
    out[2 * i + 1] = digits[data[i] & 0xF];
  }
  out[2 * len] = '\0';
}
