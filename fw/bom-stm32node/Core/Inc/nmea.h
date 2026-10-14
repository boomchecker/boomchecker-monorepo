/**
 ******************************************************************************
 * @file    nmea.h
 * @brief   The two NMEA sentences the alarm needs a position from: RMC and GGA.
 *
 * Pure parsing, no HAL, so it builds on a host as well. The talker is ignored
 * ($GPRMC, $GNRMC ... all count), a sentence must carry a valid "*hh"
 * checksum, and coordinates come out as integer microdegrees because
 * newlib-nano's printf has no floats.
 ******************************************************************************
 */
#ifndef NMEA_H
#define NMEA_H

#include <stdbool.h>
#include <stdint.h>

/** A position fix from RMC. */
typedef struct
{
  int32_t  lat_e6;   /* degrees x 1e6, north positive */
  int32_t  lon_e6;   /* degrees x 1e6, east positive */
  uint32_t utc_hms;  /* hhmmss of the fix */
  uint32_t date_dmy; /* ddmmyy of the fix, 0 when the field was empty */
} nmea_rmc_t;

/** Fix quality from GGA. */
typedef struct
{
  uint8_t  quality;  /* 0 = no fix, 1 = GPS, 2 = DGPS, ... */
  uint8_t  sats;     /* satellites used */
  uint16_t hdop_x10; /* horizontal dilution x 10, 0 when the field was empty */
} nmea_gga_t;

/** True when `line` is "$...*hh" (CR/LF allowed after hh) with a matching
 *  XOR checksum. */
bool nmea_checksum_ok(const char *line);

/** Parse an RMC sentence. True only for status A (a valid fix) with both
 *  coordinates present; `out` is left alone otherwise. */
bool nmea_parse_rmc(const char *line, nmea_rmc_t *out);

/** Parse a GGA sentence (any fix quality, 0 included). */
bool nmea_parse_gga(const char *line, nmea_gga_t *out);

#endif /* NMEA_H */
