/**
 ******************************************************************************
 * @file    nmea.c
 * @brief   RMC and GGA parsing (see nmea.h).
 ******************************************************************************
 */
#include "nmea.h"

#include <stddef.h>
#include <string.h>

static int hex_digit(char c)
{
  if (c >= '0' && c <= '9')
  {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F')
  {
    return c - 'A' + 10;
  }
  if (c >= 'a' && c <= 'f')
  {
    return c - 'a' + 10;
  }
  return -1;
}

bool nmea_checksum_ok(const char *line)
{
  if (line == NULL || line[0] != '$')
  {
    return false;
  }
  uint8_t     sum = 0u;
  const char *p   = line + 1;
  while (*p != '\0' && *p != '*')
  {
    sum ^= (uint8_t)*p;
    p++;
  }
  if (*p != '*')
  {
    return false;
  }
  const int hi = hex_digit(p[1]);
  const int lo = (hi < 0) ? -1 : hex_digit(p[2]);
  if (lo < 0)
  {
    return false;
  }
  const char end = p[3];
  if (end != '\0' && end != '\r' && end != '\n')
  {
    return false;
  }
  return (uint8_t)((hi << 4) | lo) == sum;
}

/* "$xxTYP," for any two-letter talker. */
static bool is_type(const char *line, const char *type)
{
  return line[0] == '$' && line[1] != '\0' && line[2] != '\0' &&
         strncmp(line + 3, type, 3) == 0 && line[6] == ',';
}

/* Field `idx` of the sentence (0 = "$xxTYP"): its start and length, ending at
   the next ',' or the '*' before the checksum. False past the last field. */
static bool field(const char *line, unsigned idx, const char **f, unsigned *len)
{
  const char *p = line;
  for (unsigned i = 0u; i < idx; i++)
  {
    while (*p != ',' && *p != '*' && *p != '\0')
    {
      p++;
    }
    if (*p != ',')
    {
      return false;
    }
    p++;
  }
  unsigned n = 0u;
  while (p[n] != ',' && p[n] != '*' && p[n] != '\0')
  {
    n++;
  }
  *f   = p;
  *len = n;
  return true;
}

/* Decimal "iii[.fff]" into an integer scaled by 10^frac_digits, extra
   fraction digits truncated. False on anything else, an empty field included. */
static bool parse_fixed(const char *f, unsigned len, unsigned frac_digits, uint32_t *out)
{
  uint64_t v = 0u;
  unsigned int_digits = 0u, frac = 0u;
  unsigned i = 0u;
  for (; i < len && f[i] != '.'; i++)
  {
    if (f[i] < '0' || f[i] > '9')
    {
      return false;
    }
    v = v * 10u + (uint64_t)(f[i] - '0');
    int_digits++;
  }
  /* Ten integer digits at most: with six fraction digits that still fits. */
  if (int_digits == 0u || int_digits > 10u)
  {
    return false;
  }
  if (i < len)
  {
    i++; /* '.' */
  }
  for (; i < len; i++)
  {
    if (f[i] < '0' || f[i] > '9')
    {
      return false;
    }
    if (frac < frac_digits)
    {
      v = v * 10u + (uint64_t)(f[i] - '0');
      frac++;
    }
  }
  for (; frac < frac_digits; frac++)
  {
    v *= 10u;
  }
  if (v > UINT32_MAX)
  {
    return false;
  }
  *out = (uint32_t)v;
  return true;
}

/* "ddmm.mmmm" (deg_digits 2) or "dddmm.mmmm" (3) plus the hemisphere field
   into signed microdegrees. */
static bool parse_coord(const char *f, unsigned len, unsigned deg_digits, const char *hemi,
                        unsigned hemi_len, char pos, char neg, int32_t *out)
{
  unsigned int_digits = 0u;
  while (int_digits < len && f[int_digits] != '.')
  {
    int_digits++;
  }
  if (int_digits != deg_digits + 2u || hemi_len != 1u || (hemi[0] != pos && hemi[0] != neg))
  {
    return false;
  }
  uint32_t deg = 0u;
  for (unsigned i = 0u; i < deg_digits; i++)
  {
    if (f[i] < '0' || f[i] > '9')
    {
      return false;
    }
    deg = deg * 10u + (uint32_t)(f[i] - '0');
  }
  uint32_t min_e6 = 0u; /* minutes x 1e6, below 60e6 */
  if (!parse_fixed(f + deg_digits, len - deg_digits, 6u, &min_e6) || min_e6 >= 60000000u ||
      deg > ((deg_digits == 2u) ? 90u : 180u))
  {
    return false;
  }
  const int32_t udeg = (int32_t)(deg * 1000000u + (min_e6 + 30u) / 60u);
  *out               = (hemi[0] == neg) ? -udeg : udeg;
  return true;
}

/* Fields of a sentence, by index; a missing one reads as empty. */
typedef struct
{
  const char *f[13];
  unsigned    len[13];
} fields_t;

static void split(const char *line, fields_t *fs, unsigned count)
{
  for (unsigned i = 0u; i < count; i++)
  {
    if (!field(line, i, &fs->f[i], &fs->len[i]))
    {
      fs->f[i]   = "";
      fs->len[i] = 0u;
    }
  }
}

bool nmea_parse_rmc(const char *line, nmea_rmc_t *out)
{
  if (line == NULL || out == NULL || !is_type(line, "RMC") || !nmea_checksum_ok(line))
  {
    return false;
  }
  fields_t fs;
  split(line, &fs, 10u);
  if (fs.len[2] != 1u || fs.f[2][0] != 'A')
  {
    return false;
  }
  nmea_rmc_t r;
  uint32_t   hms_e3 = 0u;
  if (!parse_coord(fs.f[3], fs.len[3], 2u, fs.f[4], fs.len[4], 'N', 'S', &r.lat_e6) ||
      !parse_coord(fs.f[5], fs.len[5], 3u, fs.f[6], fs.len[6], 'E', 'W', &r.lon_e6) ||
      !parse_fixed(fs.f[1], fs.len[1], 3u, &hms_e3))
  {
    return false;
  }
  r.utc_hms = hms_e3 / 1000u;
  if (fs.len[9] != 6u || !parse_fixed(fs.f[9], fs.len[9], 0u, &r.date_dmy))
  {
    r.date_dmy = 0u;
  }
  *out = r;
  return true;
}

bool nmea_parse_gga(const char *line, nmea_gga_t *out)
{
  if (line == NULL || out == NULL || !is_type(line, "GGA") || !nmea_checksum_ok(line))
  {
    return false;
  }
  fields_t fs;
  split(line, &fs, 9u);
  uint32_t quality = 0u, sats = 0u, hdop = 0u;
  if (!parse_fixed(fs.f[6], fs.len[6], 0u, &quality) || quality > 255u)
  {
    return false;
  }
  if (!parse_fixed(fs.f[7], fs.len[7], 0u, &sats) || sats > 255u)
  {
    sats = 0u;
  }
  if (!parse_fixed(fs.f[8], fs.len[8], 1u, &hdop) || hdop > 65535u)
  {
    hdop = 0u;
  }
  out->quality  = (uint8_t)quality;
  out->sats     = (uint8_t)sats;
  out->hdop_x10 = (uint16_t)hdop;
  return true;
}
