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

extern "C" {
#include "main.h" /* HAL_GetTick */
}

#include <cstring>

static_assert(RADIO_MAX_PAYLOAD <= RADIOLIB_SX126X_MAX_PACKET_LENGTH,
              "RADIO_MAX_PAYLOAD must not exceed the SX126x hardware packet limit");

namespace {

enum class Mode { kIdle, kReceiving, kTransmitting };

/* Upper bound on how long one transmission is ever expected to take
   (generous margin over the airtime of a maximum-size packet at this
   profile - see docs/radio-profile.md). If DIO1 never fires for a TX -
   a wiring fault, a wedged chip - radio_process() forces the radio back to
   standby/RX after this instead of leaving radio_send() permanently
   reporting "busy" and RX permanently unarmed with no way to recover short
   of a reset. */
constexpr uint32_t kTxTimeoutMs = 2000;

SX1262       *s_radio = nullptr;
Mode          s_mode  = Mode::kIdle;
volatile bool s_dio1Event = false;
uint32_t      s_txStartMs = 0;

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

/* (Re)start listening after boot, after a TX/RX completion, or after a
   forced recovery. Leaves the radio idle (no further DIO1 events expected)
   and records the failure if startReceive() itself fails - a bad SPI link
   would otherwise spin retrying forever while radio_is_ready() kept
   claiming the radio was healthy. */
void EnterReceive() {
  int16_t state = s_radio->startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    s_mode = Mode::kReceiving;
  } else {
    s_mode = Mode::kIdle;
    s_lastError = state;
  }
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
  /* s_radio is set once, permanently, on a successful radio_init() - it is
     not enough on its own: if a later EnterReceive() failed (see above),
     s_mode drops to kIdle while s_radio stays non-null, and the radio is
     actually deaf. Report that as not-ready rather than a false "healthy". */
  return s_radio != nullptr && s_mode != Mode::kIdle;
}

int radio_last_error(void) {
  return s_lastError;
}

void radio_process(void) {
  if (s_radio == nullptr) {
    return;
  }

  if (s_mode == Mode::kTransmitting && !s_dio1Event &&
      (HAL_GetTick() - s_txStartMs) > kTxTimeoutMs) {
    /* DIO1 never fired for this transmission - force the chip back to a
       known state instead of staying wedged in kTransmitting forever (which
       would make every future radio_send() fail and RX never resume). */
    s_stats.tx_errors++;
    s_lastError = RADIOLIB_ERR_TX_TIMEOUT;
    (void)s_radio->standby();
    EnterReceive();
    return;
  }

  if (!s_dio1Event) {
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
        s_lastError = state;
      }
      EnterReceive(); /* raw bring-up radio is always listening except mid-TX */
      break;
    }
    case Mode::kReceiving: {
      size_t len = s_radio->getPacketLength();
      if (len > sizeof(s_rxBuf)) {
        len = sizeof(s_rxBuf);
      }
      /* readData()'s `len` is not "read at most this many bytes and nothing
         else": SX126x::readData treats len==0 as "I don't know the length,
         figure it out yourself" rather than "read zero bytes". That only
         coincides with "this was a genuinely empty packet" by accident here
         because getPacketLength() also returns 0 for one - it stops being
         equivalent the moment a future PR shrinks RADIO_MAX_PAYLOAD (e.g.
         to leave room for BoomLink's link-frame header) enough that this
         clamp could itself produce 0. The static_assert above only checks
         the upper bound, not this. */
      int16_t state = s_radio->readData(s_rxBuf, len);
      s_stats.last_rssi_dbm = s_radio->getRSSI();
      s_stats.last_snr_db   = s_radio->getSNR();
      if (state == RADIOLIB_ERR_NONE) {
        if (s_rxPending) {
          /* radio_poll_rx() hasn't drained the previous packet yet - it is
             about to be silently replaced (see radio.h). */
          s_stats.rx_overruns++;
        }
        s_rxLen     = len;
        s_rxPending = true;
        s_stats.rx_packets++;
      } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        s_stats.rx_crc_errors++;
      } else {
        s_lastError = state;
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

  /* Flush any event already pending (e.g. a packet that finished arriving
     moments ago) against the CURRENT mode before touching it. Without this,
     a DIO1 edge raised for that earlier event could still be sitting in
     s_dio1Event when *this* transmission's own completion interrupt fires;
     radio_process() has no way to tell them apart and would either abort
     this transmission mid-air (misreading the stale flag as this TX's
     "done") or silently drop the packet that actually just arrived. */
  radio_process();

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
  s_mode      = Mode::kTransmitting;
  s_txStartMs = HAL_GetTick();
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
