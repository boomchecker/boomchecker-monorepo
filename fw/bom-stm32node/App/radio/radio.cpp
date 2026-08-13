/**
 ******************************************************************************
 * @file    radio.cpp
 * @brief   Public radio.h API implementation: owns the RadioLib SX1262/Module
 *          objects and the raw send/receive state machine. E22/board-specific
 *          bring-up lives in e22_radio.cpp; STM32 platform glue lives in
 *          stm32_radiolib_hal.cpp.
 ******************************************************************************
 */
#include "radio.h"

#include "e22_radio.h"
#include "stm32_radiolib_hal.hpp"

#include <cstring>

namespace {

enum class Mode { kIdle, kReceiving, kTransmitting };

SX1262       *s_radio = nullptr;
Mode          s_mode  = Mode::kIdle;
volatile bool s_dio1Event = false;

radio_stats_t s_stats = {};
int           s_lastError = 0;

uint8_t s_rxBuf[RADIO_MAX_PAYLOAD];
size_t  s_rxLen     = 0;
bool    s_rxPending = false;

/* RadioLib callback attached once via setDio1Action() (setPacketReceivedAction
   and setPacketSentAction are both aliases for the same single DIO1 attach
   point - see SX126x_config.cpp - so one persistent callback covers both TX-
   done and RX-done; radio_process() disambiguates using `s_mode`). Must stay
   minimal per boomlink.md section 6.2: only sets a flag, real IRQ-status/
   packet handling happens in radio_process() from the main loop. */
void OnDio1() {
  s_dio1Event = true;
}

/* (Re)start listening after boot, or after a TX/RX completion. Leaves the
   radio idle (no further DIO1 events expected) if startReceive() itself
   fails - a bad SPI link would otherwise spin retrying forever. */
void EnterReceive() {
  s_mode = (s_radio->startReceive() == RADIOLIB_ERR_NONE) ? Mode::kReceiving : Mode::kIdle;
}

} // namespace

int radio_init(void) {
  if (s_radio != nullptr) {
    return 0; /* already initialized */
  }

  e22_radio::PowerUp();

  static Module module(&stm32_radiolib_hal_instance(), RADIO_PIN_NSS, RADIO_PIN_DIO1,
                        RADIO_PIN_NRST, RADIO_PIN_BUSY);
  static SX1262 sx1262(&module);

  e22_radio::ConfigureModule(sx1262);

  const e22_radio::Profile &profile = e22_radio::DefaultProfile();
  int16_t state = sx1262.begin(profile.frequencyMhz, profile.bandwidthKhz,
                                profile.spreadingFactor, profile.codingRateDenom,
                                profile.syncWord, profile.txPowerDbm,
                                profile.preambleSymbols, profile.tcxoVoltage,
                                /*useRegulatorLDO=*/false);
  if (state != RADIOLIB_ERR_NONE) {
    s_lastError = state;
    return state;
  }

  s_radio = &sx1262;
  s_radio->setDio1Action(OnDio1);
  EnterReceive();
  s_lastError = 0;
  return 0;
}

bool radio_is_ready(void) {
  return s_radio != nullptr;
}

int radio_last_error(void) {
  return s_lastError;
}

void radio_process(void) {
  if (s_radio == nullptr || !s_dio1Event) {
    return;
  }
  s_dio1Event = false;

  switch (s_mode) {
    case Mode::kTransmitting: {
      int16_t state = s_radio->finishTransmit();
      if (state == RADIOLIB_ERR_NONE) {
        s_stats.tx_packets++;
      } else {
        s_stats.tx_errors++;
      }
      EnterReceive(); /* raw bring-up radio is always listening except mid-TX */
      break;
    }
    case Mode::kReceiving: {
      size_t len = s_radio->getPacketLength();
      if (len > sizeof(s_rxBuf)) {
        len = sizeof(s_rxBuf);
      }
      int16_t state = s_radio->readData(s_rxBuf, len);
      s_stats.last_rssi_dbm = s_radio->getRSSI();
      s_stats.last_snr_db   = s_radio->getSNR();
      if (state == RADIOLIB_ERR_NONE) {
        s_rxLen     = len;
        s_rxPending = true;
        s_stats.rx_packets++;
      } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        s_stats.rx_crc_errors++;
      }
      EnterReceive(); /* resume listening for the next packet */
      break;
    }
    case Mode::kIdle:
      break;
  }
}

int radio_send(const uint8_t *data, size_t len) {
  if (s_radio == nullptr || data == nullptr || len == 0 || len > RADIO_MAX_PAYLOAD) {
    return -1;
  }
  if (s_mode == Mode::kTransmitting) {
    /* Stop-and-wait at the raw radio layer too: the SX1262 is half-duplex and
       only one operation can be outstanding. BoomLink's own TX queue/backoff
       arrives in PR3. */
    return -1;
  }

  int16_t state = s_radio->startTransmit(data, len);
  if (state != RADIOLIB_ERR_NONE) {
    s_stats.tx_errors++;
    s_lastError = state;
    EnterReceive();
    return state;
  }
  s_mode = Mode::kTransmitting;
  return 0;
}

bool radio_poll_rx(uint8_t *buf, size_t max_len, size_t *out_len,
                    float *out_rssi_dbm, float *out_snr_db) {
  if (!s_rxPending) {
    return false;
  }
  if (buf != nullptr) {
    size_t copyLen = (s_rxLen > max_len) ? max_len : s_rxLen;
    memcpy(buf, s_rxBuf, copyLen);
  }
  if (out_len != nullptr) {
    *out_len = s_rxLen;
  }
  if (out_rssi_dbm != nullptr) {
    *out_rssi_dbm = s_stats.last_rssi_dbm;
  }
  if (out_snr_db != nullptr) {
    *out_snr_db = s_stats.last_snr_db;
  }
  s_rxPending = false;
  return true;
}

void radio_get_profile(radio_profile_t *out) {
  if (out == nullptr) {
    return;
  }
  const e22_radio::Profile &profile = e22_radio::DefaultProfile();
  out->frequency_mhz     = profile.frequencyMhz;
  out->bandwidth_khz     = profile.bandwidthKhz;
  out->spreading_factor  = profile.spreadingFactor;
  out->coding_rate_denom = profile.codingRateDenom;
  out->tx_power_dbm      = profile.txPowerDbm;
  out->preamble_symbols  = profile.preambleSymbols;
  out->sync_word         = profile.syncWord;
}

void radio_get_stats(radio_stats_t *out) {
  if (out == nullptr) {
    return;
  }
  *out = s_stats;
}

void radio_reset_stats(void) {
  s_stats = radio_stats_t{};
}
