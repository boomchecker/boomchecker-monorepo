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
 ******************************************************************************
 */
#ifndef GPS_H
#define GPS_H

#include <stdint.h>

#define GPS_DEFAULT_BAUD 9600u
#define GPS_MAX_SECONDS  300u

/** Stream NMEA lines from the module to the console for `seconds`.
 *  Emits "GPS baud=..." first, raw NMEA lines, then "GPSEND lines=... " */
void gps_run(uint32_t seconds, uint32_t baud);

/** Send one sentence to the module at `baud`. `sentence` may omit the
 *  leading '$'; the NMEA checksum and CRLF are appended here.
 *  @return 0 on success, non-zero on UART error. */
int gps_send(const char *sentence, uint32_t baud);

/** Pulse SYS_RSTn (GPS_RESET, active low) low for 100 ms to restart the
 *  module. Bring-up fallback for a module that stays silent. */
void gps_reset_pulse(void);

#endif /* GPS_H */
