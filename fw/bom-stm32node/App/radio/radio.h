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

/* Largest payload radio_send()/radio_poll_rx() will move in one LoRa packet -
   the SX126x's hard packet-length ceiling (RADIOLIB_SX126X_MAX_PACKET_LENGTH),
   not a value we chose. */
#define RADIO_MAX_PAYLOAD 255u

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
  uint32_t rx_overruns; /* a new packet finished decoding before radio_poll_rx()
                            drained the previous one - see radio_poll_rx(). */
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
 * @return the status code from the most recent radio_init() or radio_send()
 * failure (a RadioLib status code, or -1), or 0 if the last such call
 * succeeded. For CLI/diagnostic display - see the `radio status` command.
 */
int radio_last_error(void);

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
 *
 * Queued, single-consumer: a small fixed-depth ring holds packets that
 * finished decoding before this was called again, so a burst of a few
 * near-simultaneous transmissions survives between polls rather than all but
 * the latest being silently lost. The ring is still bounded - a packet
 * arriving when it is full is dropped and counted in
 * radio_stats_t::rx_overruns, same as ever, just at a higher bar.
 *
 * Still single-consumer, and that part has NOT changed: two independent
 * callers polling this from the same superloop would each only ever drain
 * some of the queue, never see all of it, since every call pops the oldest
 * buffered packet regardless of who is asking. Exactly one caller may poll
 * this at a time - see App/link/link_service.h and cli.c's `link
 * enable`/`link disable`, which arbitrate that between BoomLink and the raw
 * bring-up RX preview so only one of them ever does.
 */
bool radio_poll_rx(uint8_t *buf, size_t max_len, size_t *out_len,
                    float *out_rssi_dbm, float *out_snr_db);

/** Fill *out with the active (fixed, bring-up) LoRa PHY profile. */
void radio_get_profile(radio_profile_t *out);

/**
 * Estimated time on air, in microseconds, for a `len`-byte packet under the
 * ACTIVE profile - RadioLib's PhysicalLayer::getTimeOnAir(len), which this
 * layer computes but until now never exposed (see boomlink_port.h's
 * airtime_us comment, which names this gap explicitly). Returns 0 if the
 * radio is not ready, the same "nothing to report" sentinel radio_send()'s
 * own error path implies - never a plausible-looking duration for a packet
 * that cannot actually be sent right now.
 */
uint32_t radio_airtime_us(size_t len);

/** Fill *out with a snapshot of the running link statistics. */
void radio_get_stats(radio_stats_t *out);

/** Zero all counters (last RSSI/SNR included). */
void radio_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* RADIO_H */
