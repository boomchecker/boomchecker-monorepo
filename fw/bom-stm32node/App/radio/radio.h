/**
 ******************************************************************************
 * @file    radio.h
 * @brief   Public C API for the LoRa radio (SX1262). Application code must
 *          only ever include this header, never RadioLib or e22_radio types
 *          directly - see docs/firmware/bom-stm32node/boomlink.md section 5.1
 *          and 16 (agent rule 3).
 ******************************************************************************
 */
#ifndef RADIO_H
#define RADIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Largest payload radio_send()/radio_poll_rx() will move in one LoRa packet. */
#define RADIO_MAX_PAYLOAD 256u

typedef struct {
  float    frequency_mhz;
  float    bandwidth_khz;
  uint8_t  spreading_factor;
  uint8_t  coding_rate_denom;
  int8_t   tx_power_dbm;
  uint16_t preamble_symbols;
  uint8_t  sync_word;
} radio_profile_t;

typedef struct {
  uint32_t tx_packets;
  uint32_t tx_errors;
  uint32_t rx_packets;
  uint32_t rx_crc_errors;
  float    last_rssi_dbm;
  float    last_snr_db;
} radio_stats_t;

/**
 * Bring up the SPI1-attached SX1262: EN_LORA power sequencing, RadioLib
 * begin() with the bring-up LoRa profile (radio_get_profile()), RF-switch
 * wiring, private sync word, then start listening. Safe to call once at
 * boot. Never calls Error_Handler(): on failure the radio stays disabled and
 * every other radio_*() call becomes a harmless no-op, so USB/CLI/microphone
 * keep working without a radio attached or wired correctly.
 * @return 0 on success, a RadioLib status code (negative) otherwise.
 */
int radio_init(void);

/** @return true once radio_init() has completed successfully. */
bool radio_is_ready(void);

/**
 * Service the radio from the main superloop: drains the DIO1 event flag set
 * by the EXTI ISR and drives RadioLib's packet TX/RX completion handling.
 * Must be called frequently (every superloop iteration) and never from
 * interrupt context - see boomlink.md section 6.2.
 */
void radio_process(void);

/**
 * Transmit `len` bytes (<= RADIO_MAX_PAYLOAD) as one raw LoRa packet. This is
 * a raw, unaddressed, unacknowledged send for bring-up testing only -
 * BoomLink's addressing/ACK/retry/TX queue arrives in PR3.
 * @return 0 if accepted for transmission; non-zero (a RadioLib status code,
 *         or -1 if not ready/already transmitting/oversized) otherwise.
 */
int radio_send(const uint8_t *data, size_t len);

/**
 * Non-blocking receive poll. If a new packet has arrived since the last
 * call, copies up to `max_len` bytes of it into `buf`, fills *out_len with
 * the packet's actual length (which may exceed max_len) and *out_rssi_dbm/
 * *out_snr_db with its signal quality, then returns true. Returns false if
 * nothing new has arrived. Any of `buf`/out_len/out_rssi_dbm/out_snr_db may
 * be NULL.
 */
bool radio_poll_rx(uint8_t *buf, size_t max_len, size_t *out_len,
                    float *out_rssi_dbm, float *out_snr_db);

/** Fill *out with the active (fixed, bring-up) LoRa PHY profile. */
void radio_get_profile(radio_profile_t *out);

/** Fill *out with a snapshot of the running link statistics. */
void radio_get_stats(radio_stats_t *out);

/** Zero all counters (last RSSI/SNR included). */
void radio_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* RADIO_H */
