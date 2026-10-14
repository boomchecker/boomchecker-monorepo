/**
 ******************************************************************************
 * @file    gps.h
 * @brief   Teseo-LIV3R GNSS bring-up: NMEA passthrough over the USB console.
 *
 * The module talks NMEA on UART4 (module TX -> PB8/UART4_RX). gps_run()
 * re-inits UART4 at the requested baud rate, collects bytes via an RX
 * interrupt ring and prints complete NMEA lines on the CDC console, ending
 * with a "GPSEND ..." trailer. gps_send() writes one sentence to the module
 * (NMEA checksum appended). The Teseo-LIV3R is a ROM part whose config does
 * not persist without VBAT, so the host adapts to the module's default
 * 9600 Bd rather than reconfiguring it.
 *
 * Both gps_run() and the background tracking during `detect` parse RMC/GGA
 * (nmea.h) and keep the last valid fix for gps_last_fix().
 ******************************************************************************
 */
#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include <stdint.h>

#define GPS_DEFAULT_BAUD 9600u
#define GPS_MAX_SECONDS  300u

/** Stream NMEA lines from the module to the console for `seconds`.
 *  Emits "GPS baud=..." first, raw NMEA lines, then "GPSEND lines=... ".
 *  A reply buffered by a preceding gps_send() comes out first. Reception is
 *  switched off when the run ends, so nothing stale accumulates in idle. */
void gps_run(uint32_t seconds, uint32_t baud);

/** Send one sentence to the module at `baud`. `sentence` may omit the
 *  leading '$'; the NMEA checksum and CRLF are appended here. The reply waits
 *  in the ring for the next gps_run(), behind those of any earlier sends.
 *  @return 0 on success, non-zero on UART error. */
int gps_send(const char *sentence, uint32_t baud);

/** Pulse SYS_RSTn (GPS_RESET, active low) low for 100 ms to restart the
 *  module. Bring-up fallback for a module that stays silent. */
void gps_reset_pulse(void);

/** The last valid position (RMC status A) that `gps` or a tracked `detect`
 *  run saw since boot. */
typedef struct
{
  bool     valid;    /* false until the first fix */
  int32_t  lat_e6;   /* degrees x 1e6, north positive */
  int32_t  lon_e6;   /* degrees x 1e6, east positive */
  uint32_t utc_hms;  /* hhmmss of the fix */
  uint32_t date_dmy; /* ddmmyy of the fix, 0 if the module sent none */
  uint8_t  sats;     /* from the latest GGA with a fix, 0 if none */
  uint16_t hdop_x10; /* likewise, HDOP x 10 */
  uint32_t tick_ms;  /* HAL_GetTick() when the fix arrived */
} gps_fix_t;

/** Listen to the module in the background at GPS_DEFAULT_BAUD while another
 *  command runs (`detect`): bytes collect in the RX ring, gps_track_poll()
 *  parses them - call it at least every ~0.5 s, the ring holds about a
 *  second - and gps_track_stop() switches reception off again.
 *  @return 0 on success, non-zero on UART error. */
int  gps_track_start(void);
void gps_track_poll(void);
void gps_track_stop(void);

/** Copy the last fix into `out` (may be NULL); returns gps_fix_t.valid. */
bool gps_last_fix(gps_fix_t *out);

#endif /* GPS_H */
