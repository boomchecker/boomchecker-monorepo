# Bring-up checklist — BCH Node v0.1

Test list for the **first revision** of the `bch-stmnode_v0.1` board. The goal is
not to produce datasheet-grade measurements, but to find out **what on the board
is actually alive** and what has to be fixed in `v0.2`. Every test therefore
carries both a "pass criterion" and a "tolerable" column — plenty of things
(weaker GNSS signal, lower microphone sensitivity, shorter LoRa range) are
acceptable in a first revision as long as they work in principle and can be
recovered in firmware or with a small rework.

Sources: `bch-stmnode_v0.1.PrjPcb`, `outputs/bom_bch-stmnode_v0.1.xlsx`,
`outputs/schema_boomchecker-stmnode_v0.1.PDF`, pin map from
`fw/bom-stm32node/bom-stm32node.ioc` (careful — it is a draft, see
[§13](#13-discrepancies-to-resolve-against-the-schematic)).

## How to use this

- Run the tests **in order** — sections 0–2 are a gate: until they pass, testing
  peripherals is pointless. Within sections 3–9 the order can be changed.
- **P0** = blocks further work, **P1** = needed for the unit to do its job,
  **P2** = nice to have / can slip to v0.2.
- 🔧 = **no firmware exists for this today**, the test needs new bring-up code
  (see [§14](#14-firmware-still-to-be-written)). Tests without the mark can be
  run with today's `fw/bom-stm32node` (`task build`, `task flash`, CLI over CDC).
- Record results in the table in [§15](#15-test-record); on a fail, open an issue
  with the `hw` label referencing the test ID (e.g. `GPS-3`).

### What you need on the bench

Multimeter, lab supply with current limit (5 V / 0.5 A), USB-C cable, ST-Link V3
(SWD on J3, FTSH-107 2×7 1.27 mm), oscilloscope or logic analyzer (50 MHz
minimum — the PDM clock is 3.072/6.144 MHz), Li-Ion cell with a JST-ZR connector
(J4), 2× board for the LoRa test, `onemic v0.1` microphone boards + a 12p /
0.5 mm FFC cable (J11), a microSD card, sky view for GNSS (a window is not
enough for the first fix), and a loud impulse source (hand clap, balloon pop).

---

## 0. Before power is applied for the first time (P0)

| ID | Test | Pass criterion |
|---|---|---|
| PRE-1 | Visual inspection against `cpm_bch-stmnode_v0.1.csv` — orientation of U4 (STM32H563ZIT6), U1 (Teseo-LIV3R), U6 (E22-900M22S), U2/U3 (LGA), D1 polarity, tantalum/electrolytic polarity | Nothing reversed, nothing missing versus the BOM |
| PRE-2 | Resistance GND ↔ +3V3, GND ↔ VBAT, GND ↔ VBUS (5 V) with an ohmmeter | No short (single-digit Ω = stop), typically tens of kΩ or more |
| PRE-3 | J5 (USB-C), J6 (microSD), J10/J11 (FFC) under a magnifier — solder bridges | No bridges |
| PRE-4 | S2 (slide switch) in OFF / no battery connected | — |

- [ ] PRE-1 [ ] PRE-2 [ ] PRE-3 [ ] PRE-4

---

## 1. Power and charging (P0)

Populated: J5 USB-C (USB4720-03-A) → F1 (0.75 A fuse) → U7 (USBLC6-2SC6Y ESD),
U9 (TPS563252DRLR buck, L4 2.2 µH) → +3V3, U8 (BQ21040DBVR Li-Ion charger),
J4 (JST S2B-ZR battery), S2 (switch), D1 (green LED), U12/U13 (TPS22919 load
switches for LoRa and the IMU).

| ID | Test | Procedure | Pass criterion | Tolerable |
|---|---|---|---|---|
| PWR-1 | First power-on from the lab supply | 5 V on VBUS with a 100 mA limit, no firmware | Current < 50 mA, nothing gets warm | — |
| PWR-2 | Buck U9 output | Multimeter on +3V3 | 3.3 V ±3 % | ±5 % with a note |
| PWR-3 | +3V3 ripple | Scope in AC, 20 MHz BW limit | < 50 mV p-p | < 100 mV p-p (but record it — it affects the ADC/mics) |
| PWR-4 | U9 switching frequency | Scope on SW / L4 | ~1.2 MHz, no jumping into burst mode under load | — |
| PWR-5 | Battery charging, U8 | Cell on J4, USB connected | Charge current flows, cell voltage rises, U8 stays below ~60 °C | Lower current than designed |
| PWR-6 | Running from battery | Unplug USB, S2 to ON | Board keeps running, +3V3 stable until the cell is depleted | — |
| PWR-7 | Battery ↔ USB transition | Plug/unplug USB while running | MCU does not reset | A reset on switchover = record it for v0.2 |
| PWR-8 | S2 function | Toggle OFF/ON | Board powers down/up as the schematic intends | — |
| PWR-9 | D1 (green LED) | After power-on | Lights up / blinks per the schematic (power/charge indication) | — |
| PWR-10 🔧 | LoRa load switch (U12, `EN_LORA` = PE7) | Toggle the GPIO, measure current | E22 supply can be switched both off and on | — |
| PWR-11 🔧 | IMU load switch (U13, `EN_IMU` = PG14) | Same | IMU/MAG supply is controllable | — |
| PWR-12 | Consumption in the basic states | idle / GNSS tracking / LoRa TX / recording | Numbers written down — input for battery life and v0.2 | Any value, the point is having it measured |
| PWR-13 | Battery voltage into the MCU | Check whether an ADC divider exists | **We expect it is missing** — then measure with a multimeter and record it as a v0.2 requirement | — |

- [ ] PWR-1 [ ] PWR-2 [ ] PWR-3 [ ] PWR-4 [ ] PWR-5 [ ] PWR-6 [ ] PWR-7 [ ] PWR-8 [ ] PWR-9 [ ] PWR-10 [ ] PWR-11 [ ] PWR-12 [ ] PWR-13

---

## 2. MCU, clocks, debug, console (P0)

| ID | Test | Procedure | Pass criterion | Tolerable |
|---|---|---|---|---|
| MCU-1 | SWD connection | ST-Link on J3, `task flash` in `fw/bom-stm32node` | OpenOCD finds the STM32H5, flash + verify pass | — |
| MCU-2 | Reset button S1 | Press it | MCU restarts (firmware comes up again) | — |
| MCU-3 | HSE start-up (Y1) | Debugger: HSE ready flag, optionally MCO/pin on the scope | Crystal starts, no retry | — |
| MCU-4 | **Y1 frequency vs. configuration** | From the scope or the BOM: Y1 = 24 MHz, but the `.ioc` says `HSE_VALUE=25000000` | Reconcile the two — otherwise every derived clock is wrong (UART baud, SAI, USB) | Nothing, this is a must-fix |
| MCU-5 | LSE start-up (Y2, 32.768 kHz) | Debugger: LSE ready | Starts within a few hundred ms | If it does not, take RTC/timestamps from GNSS |
| MCU-6 | SYSCLK 250 MHz | Verify for real (MCO, or timing a known loop / GPIO toggle) | Matches the configuration; the PDM DSP keeps up at 250 MHz | — |
| MCU-7 | USB CDC enumeration | Plug USB-C into a PC | A CDC ACM appears, product string `boomchecker-node` | — |
| MCU-8 | Console | `version` on the CDC port | Prints the firmware version, the `> ` prompt responds | — |
| MCU-9 | USART1 VCP (PB14/PB15) | Wire to the pins / ST-Link VCP | Text passes in both directions | P2 — it is a backup for the CDC |
| MCU-10 | U4 temperature under load | Finger / thermal camera after 10 min of recording | No abnormal heating | — |

- [ ] MCU-1 [ ] MCU-2 [ ] MCU-3 [ ] MCU-4 [ ] MCU-5 [ ] MCU-6 [ ] MCU-7 [ ] MCU-8 [ ] MCU-9 [ ] MCU-10

---

## 3. GNSS — Teseo-LIV3R (P1)

Signal path: J1 (18×18 THM patch antenna) → U10 (SAW B39162B4327P810) →
U5 (AT2659S LNA) → U1 (Teseo-LIV3R). Interfaces: I2C1 (`GPS_SCL` PB6 /
`GPS_SDA` PB7), UART4 (`GPS_TX` PB8 / `GPS_RX` PB9), `GPS_1PPS` PG15,
`GPS_RESET` PB5, `GPS_WAKE-UP` PB4.

| ID | Test | Procedure | Pass criterion | Tolerable |
|---|---|---|---|---|
| GPS-1 🔧 | Module is alive | Reset via PB5, read NMEA from UART4 (19 200/9 600 Bd depending on the ROM configuration) | Valid NMEA sentences arrive (`$GPTXT`, `$GxGGA`) | — |
| GPS-2 🔧 | I2C path | Scan I2C1, read the same data over I2C | Module answers at its address | If it does not, carry on over UART (P2) |
| GPS-3 🔧 | **Cold-start fix outdoors** | Board under open sky, wait for `$GxGGA` with fix quality ≥ 1 | Fix within ~5 min of a cold start, position within tens of metres of reality | Fix within 15 min, worse accuracy — the point is that a fix happens |
| GPS-4 🔧 | Signal quality | `$GxGSV`: C/N0 of tracked satellites, satellites in the fix | ≥ 6 satellites, best C/N0 ≥ 40 dB-Hz | 35–40 dB-Hz and ≥ 4 satellites = "weaker but survivable"; record it as an antenna/LNA topic for v0.2 |
| GPS-5 | LNA/SAW supply | Measure the current into U5, check voltage on the antenna pin | LNA powered, current matches the datasheet (~4 mA) | — |
| GPS-6 🔧 | 1PPS exists | Scope on PG15 after a fix | One pulse per second, clean edge | — |
| GPS-7 🔧 | 1PPS as an interrupt | EXTI on PG15, count pulses | Exactly 1 Hz, no doubles or bounce (otherwise add filtering/hysteresis) | — |
| GPS-8 🔧 | 1PPS jitter | Compare against a timer on the 250 MHz clock, statistics over 100 pulses | Jitter in the single-digit µs range | Tens of µs — record it, TDOA resolution suffers |
| GPS-9 🔧 | Reset and wake-up pins | Toggle PB5 / PB4 | Module reacts (NMEA restarts, wakes up) | — |
| GPS-10 🔧 | Hot start | Restart the MCU with GNSS running | Fix back within a few seconds | — |

- [ ] GPS-1 [ ] GPS-2 [ ] GPS-3 [ ] GPS-4 [ ] GPS-5 [ ] GPS-6 [ ] GPS-7 [ ] GPS-8 [ ] GPS-9 [ ] GPS-10

---

## 4. LoRa — E22-900M22S / SX1262 (P1)

Interface: SPI1 (`LORA_SCK` PA5, `MISO` PA6, `MOSI` PA7, `NSS` PA4), `BUSY` PB1,
`DIO1` PB2, `DIO2` PF11, `NRST` PC4, `TXEN` PB0, `RXEN` PC5, `EN_LORA` PE7.
Bring-up profile: 869.525 MHz / BW 125 kHz / SF7 / CR 4/5 / 14 dBm
(`fw/bom-stm32node/docs/radio-profile.md`).

> ⚠️ **Never transmit without an antenna** — the E22 has a PA, and an unloaded
> output risks the module. Transmit only after confirming an antenna or a 50 Ω
> load is attached.

| ID | Test | Procedure | Pass criterion | Tolerable |
|---|---|---|---|---|
| LORA-1 | Power and reset | `EN_LORA` on, `NRST` sequence | Module wakes up, `BUSY` goes low | — |
| LORA-2 | SPI communication | `radio status` on the CLI | `radio: ready`, prints the profile (869.525 MHz, SF7, …) | — |
| LORA-3 | TCXO | Part of `begin()` (DIO3 at 1.8 V) | Init passes without `RADIOLIB_ERR_SPI_CMD_TIMEOUT` / XOSC errors | — |
| LORA-4 | TX/RX switch | Watch `TXEN`/`RXEN` during TX and RX | They switch correctly, never both active at once | — |
| LORA-5 | `radio ping` — single board | `radio ping TEST` | `sent "TEST" (4 bytes)`, no error | — |
| LORA-6 | **Ping/pong, two boards** | `radio ping` on one, watch for the `radio rx: …` line on the other | Packet arrives in both directions, CRC OK | — |
| LORA-7 | DIO1 IRQ | EXTI on PB2 | TX done / RX done arrive as interrupts, not by polling | Polling as a fallback (P2) |
| LORA-8 | RSSI/SNR on the bench | `radio status` after a receive, boards ~1 m apart | Plausible RSSI (strong, tens of -dBm), SNR > 5 dB | — |
| LORA-9 | Range | 2 boards, ~100 m open space, then with one obstacle | Packet ratio ≥ 90 % at 100 m | Weaker range is fine in rev 0.1 — record the numbers, do not over-read them |
| LORA-10 | TX current | Measure the peak at 14 dBm TX | Matches the datasheet, +3V3 does not sag and the MCU does not reset | — |
| LORA-11 | Output power | If a power meter / SDR + attenuator is available | Confirm we stay inside the sub-band limit (see `radio-profile.md`) | Can be deferred, but do not forget it |

- [ ] LORA-1 [ ] LORA-2 [ ] LORA-3 [ ] LORA-4 [ ] LORA-5 [ ] LORA-6 [ ] LORA-7 [ ] LORA-8 [ ] LORA-9 [ ] LORA-10 [ ] LORA-11

---

## 5. IMU — ISM330DLCTR (P1)

SPI3 (`IMU_SCK` PC10, `MISO`/SDO PC11, `MOSI`/SDI PC12), `IMU_CS` PD0,
`IMU_INT1` PD1, `IMU_INT2` PD3, supply through a load switch (`EN_IMU` PG14).

| ID | Test | Procedure | Pass criterion | Tolerable |
|---|---|---|---|---|
| IMU-1 🔧 | WHO_AM_I | Read register `0x0F` over SPI3 | Returns `0x6B` | — |
| IMU-2 🔧 | Accelerometer — static | Board flat on the bench | One axis reads ~±1 g, the others ~0; flipping the board flips the sign | — |
| IMU-3 🔧 | Gyroscope | Board at rest / rotated by hand | ~0 °/s at rest (bias of a few °/s), the correct axis responds to rotation | — |
| IMU-4 🔧 | Data rate | Set the ODR, measure the actual sample period | Matches the setting | — |
| IMU-5 🔧 | INT1/INT2 | Configure data-ready / wake-up | The interrupt really arrives on both PD1 and PD3 | If one pin is dead, one is enough to start |
| IMU-6 🔧 | Power-down via the load switch | `EN_IMU` low → high, re-init | The device answers again after power-up (clean power cycle) | — |

- [ ] IMU-1 [ ] IMU-2 [ ] IMU-3 [ ] IMU-4 [ ] IMU-5 [ ] IMU-6

---

## 6. Magnetometer — LIS2MDLTR (P2)

Per the schematic and the `.ioc` it sits on **I2C2** (`MAG_SDA` PF0, `MAG_SCL`
PF1) — the board README claimed SPI, see
[§13](#13-discrepancies-to-resolve-against-the-schematic).

| ID | Test | Procedure | Pass criterion | Tolerable |
|---|---|---|---|---|
| MAG-1 🔧 | Scan I2C2 | Address `0x1E` | The device answers | — |
| MAG-2 🔧 | WHO_AM_I | Register `0x4F` | `0x40` | — |
| MAG-3 🔧 | Field measurement | Read all 3 axes at rest | Vector magnitude ~25–65 µT (Earth's field) | — |
| MAG-4 🔧 | Response to a magnet | Bring a magnet close | Readings swing hard and come back | — |
| MAG-5 🔧 | Board influence | Compare readings with and without LoRa TX | Record the offset from currents/metal — calibration is firmware work | A large offset is acceptable for v0.1 |

- [ ] MAG-1 [ ] MAG-2 [ ] MAG-3 [ ] MAG-4 [ ] MAG-5

---

## 7. PDM microphones (J11 + `onemic v0.1`) (P0 — the unit's core function)

J11 (12p FFC, 52745-1297) carries `PDM_CLK`, `PDM_D1`, `PDM_D2`, `PDM_D3`,
`LR_SEL`, +3V and GND to the `hw/onemic/v0.1` boards. MCU side: SAI1 (`PDM_CLK`
PE2, `PDM_D1` PE6, `PDM_D2` PE4, `PDM_D3` PF10) + GPDMA, DSP chain CIC5 → DC
block → 101-tap FIR → 48 kHz PCM (`Core/Src/mic.c`, `pdm_pcm.c`).

| ID | Test | Procedure | Pass criterion | Tolerable |
|---|---|---|---|---|
| MIC-1 | USB chain without the hardware | `stm32node_cli record 5 --test-tone` | A WAV with a clean 1 kHz tone — confirms USB/framing before you start chasing the mic | — |
| MIC-2 | Microphone board supply | Multimeter on +3V at J11 / `onemic` | 3.3 V at the microphones | — |
| MIC-3 | PDM clock | Scope on `PDM_CLK` (PE2) | Clock runs, frequency matches the SAI configuration (3.072 / 6.144 MHz), full amplitude, edges still clean at the far end of the FFC | — |
| MIC-4 | **Record audio (D1)** | `record 5 --port /dev/ttyACM*`, then play the WAV | The room is audible in the WAV — not noise, not silence | — |
| MIC-5 | Noise floor | Record in a quiet room, compute RMS / noise floor | Quiet signal, no 50 Hz hum, no whine from the switching regulator | Slightly raised noise floor is fine, record the value |
| MIC-6 | Impulse (hand clap) | Clap near the microphone | Sharp impulse followed by a quick recovery — no stuck DC offset | — |
| MIC-7 | Saturation / clipping | Loud source up close | Establish where the signal clips; it recovers from clipping | — |
| MIC-8 | Overruns | Record for 60 s | No SAI overrun, no dropped blocks (continuous PCM) | — |
| MIC-9 🔧 | Second channel on D1 (`LR_SEL`) | Toggle `LR_SEL`, read the other slot | Both microphones come off a single data line | If not, we run 3 channels instead of 6 |
| MIC-10 🔧 | D2 and D3 | Extend SAI to `PDM_D2` (PE4) and `PDM_D3` (PF10) | Data from both other lines at the same level as D1 | — |
| MIC-11 🔧 | **Inter-channel phase coherence** | One impulse, record several channels at once, compare onsets | Channels are sample-aligned to each other (constant offset) — a precondition for TDOA | A constant, known offset can be compensated; an unstable offset is a must-fix |
| MIC-12 | FFC mechanics | Mate/unmate 5× | The ZIF holds, contact does not drop when the cable is flexed | — |

- [ ] MIC-1 [ ] MIC-2 [ ] MIC-3 [ ] MIC-4 [ ] MIC-5 [ ] MIC-6 [ ] MIC-7 [ ] MIC-8 [ ] MIC-9 [ ] MIC-10 [ ] MIC-11 [ ] MIC-12

---

## 8. Analog microphones — OPA2607 + ADC (P1)

U11 (OPA2607IDR, dual op-amp) forms two channels, `MIC1_I → MIC1_O` and
`MIC2_I → MIC2_O`, whose outputs go to the ADC (PA0 = `MIC1_O`, PA1 = `MIC2_O`).
The inputs come in on J10 (6p FFC, 52745-0696). Firmware today has only the
CubeMX ADC1/ADC2 init, no sampling.

| ID | Test | Procedure | Pass criterion | Tolerable |
|---|---|---|---|---|
| AMIC-1 | U11 supply and quiescent point | Measure `MIC1_O`/`MIC2_O` with no signal | Stable DC bias mid-way through the ADC range (not 0 V, not a rail) | A shifted bias costs range — record it for v0.2 |
| AMIC-2 | Gain | Inject a known signal (generator, mV level) into J10 | Gain matches the design (resistor ratio), no oscillation | ±30 % is fine |
| AMIC-3 | Noise with no microphone attached | Scope / ADC on the output | No ringing, no op-amp oscillation (the OPA2607 is fast — watch for HF instability) | — |
| AMIC-4 🔧 | ADC read | Single conversion on PA0/PA1 | Values match what is on the pin | — |
| AMIC-5 🔧 | Audio sampling | ADC + timer + DMA at ~48 kHz, capture a block | The samples contain audio, not noise | — |
| AMIC-6 | Microphone supply on J10 | Measure the bias/supply for the analog microphones | Matches what the chosen microphone needs | — |
| AMIC-7 | Switching-regulator crosstalk | Record with the buck running under load | No tone at the switching frequency or its harmonics in the audio band | Record the level, fix in v0.2 |

- [ ] AMIC-1 [ ] AMIC-2 [ ] AMIC-3 [ ] AMIC-4 [ ] AMIC-5 [ ] AMIC-6 [ ] AMIC-7

---

## 9. microSD card (P1)

J6 (105162-0001, push-pull), SPI5 (`SD_SCK` PF7, `SD_MISO` PF8, `SD_MOSI` PF9),
`SD_CS` PF3, `SD_DET` PF2.

| ID | Test | Procedure | Pass criterion | Tolerable |
|---|---|---|---|---|
| SD-1 | Mechanics | Insert/eject a card 5× | Push-pull holds, the card does not fall out | — |
| SD-2 🔧 | Card detect | Read PF2 with and without a card | The state changes correctly (check the polarity too) | If it does not work, detect in software during init |
| SD-3 🔧 | Card initialization | SPI init sequence, CMD0/CMD8/ACMD41, read CID/CSD | The card responds, capacity can be read | — |
| SD-4 🔧 | Block read/write | Write a known pattern and read it back | Data matches | — |
| SD-5 🔧 | FAT | Mount the filesystem, create a file | The file is readable on a PC | — |
| SD-6 🔧 | Write throughput | Write continuously, measure MB/s | Handles the data rate of the planned channel count with headroom | Lower speed = fewer channels / shorter windows, record it |
| SD-7 🔧 | Writing during LoRa TX | Combine writing and transmitting | No write errors during current peaks | — |

- [ ] SD-1 [ ] SD-2 [ ] SD-3 [ ] SD-4 [ ] SD-5 [ ] SD-6 [ ] SD-7

---

## 10. USB (P0)

`USB_DM` PA11 / `USB_DP` PA12, J5 USB-C, protection by U7.

| ID | Test / pass criterion | Tolerable |
|---|---|---|
| USB-1 | The CDC appears on both Linux and Windows, product string `boomchecker-node` | — |
| USB-2 | The console stays responsive for ≥ 30 min without hanging | — |
| USB-3 | Re-enumeration after `task flash` and after an S1 reset | Having to re-run `usbipd attach` is known behaviour, not a bug |
| USB-4 | PCM streaming: `record 30` with no drops or timeouts | An occasional retry is fine, lost blocks are not |
| USB-5 | Behaviour when running on battery and USB data is plugged in | No restart |
| USB-6 | Cable in both USB-C orientations | Both must work |

- [ ] USB-1 [ ] USB-2 [ ] USB-3 [ ] USB-4 [ ] USB-5 [ ] USB-6

---

## 11. System / integration tests (P1)

This is where most first-revision problems show up — peripherals that work fine
on their own tend to break once they all run together.

| ID | Test | Pass criterion | Tolerable |
|---|---|---|---|
| SYS-1 | Recording from the microphone **while** LoRa transmits | TX is not audible in the audio (no clicking in the packet rhythm), recording does not break | Record a weak artefact for v0.2 (shielding/filtering) |
| SYS-2 | GNSS fix holds **during** LoRa TX | Satellite count and C/N0 do not drop dramatically (869 MHz vs. 1575 MHz — watch the harmonics) | A brief dip is fine |
| SYS-3 | GNSS fix holds during recording and SD writes | The fix is not lost | — |
| SYS-4 | Everything at once (GNSS + LoRa RX + PDM + SD) for 30 min | No resets, no overruns, no lost fix | — |
| SYS-5 | 1PPS timestamps over the audio stream | Samples can be tied to absolute time — the basis for TDOA between units | Can wait for the firmware phase, but confirm the hardware allows it |
| SYS-6 | Battery life on a typical profile | Measure the hours of operation — input for sizing the cell | Any number, as long as it is measured |
| SYS-7 | Behaviour on a depleted battery | The board behaves predictably (shuts down, does not hang mid-write to the SD card) | — |
| SYS-8 | Cold start after removing all power | Comes up without manual intervention | — |
| SYS-9 | Thermal check after 30 min at full load | Nothing runs hot (U9, U6, U8 in particular) | — |

- [ ] SYS-1 [ ] SYS-2 [ ] SYS-3 [ ] SYS-4 [ ] SYS-5 [ ] SYS-6 [ ] SYS-7 [ ] SYS-8 [ ] SYS-9

---

## 12. Mechanics and connectors (P2)

- [ ] MECH-1 Mounting holes M1–M4 (3 mm) match the intended fixture
- [ ] MECH-2 J5 (USB-C) is accessible and holds with a cable plugged in
- [ ] MECH-3 S1 (reset) and S2 (switch) can be operated inside the enclosure
- [ ] MECH-4 The battery cable to J4 is not pinched anywhere and the connector holds
- [ ] MECH-5 The J1 GNSS patch antenna faces the sky sensibly and is not shadowed by metal or the battery
- [ ] MECH-6 LoRa antenna: placement and cable do not disturb GNSS and have no sharp bends
- [ ] MECH-7 The microphone FFC has enough length and a defined bend (mechanical vibration coupled into the microphones = artefacts)

---

## 13. Discrepancies to resolve against the schematic

Found by cross-checking the BOM, the schematic and `bom-stm32node.ioc` — resolve
these **before** firmware is tuned against them:

- [ ] **HSE crystal**: the BOM has Y1 = 24.0 MHz, the `.ioc` has
  `HSE_VALUE=25000000` (and `EPOD_VALUE=25000000`). One of them is wrong — it
  affects SYSCLK, USB, SAI and baud rates. See test MCU-4.
- [ ] **Magnetometer**: `hw/node/v0.1/README.md` listed the LIS2MDL on SPI, while
  both the schematic and the `.ioc` have `MAG_SCL`/`MAG_SDA` (I2C2). README
  fixed alongside this checklist.
- [ ] **GNSS**: both interfaces are routed in the schematic (I2C1 and UART4).
  Decide which one is primary for firmware and confirm the Teseo's ROM
  configuration actually answers on it.
- [ ] **The pin map is not the source of truth**: per the README, the `.ioc` is a
  draft. After bring-up, generate/verify a netlist from Altium and reconcile the
  `.ioc` with it.
- [ ] **Battery voltage sensing**: the only ADC inputs in the `.ioc` are
  `MIC1_O`/`MIC2_O`. If the board has no VBAT divider, record it as a v0.2
  requirement (see PWR-13).
- [ ] **LoRa antenna**: check how the E22 output is terminated (IPEX/stamp) and
  make sure it is never transmitted into an open output.

---

## 14. Firmware still to be written

What works in `fw/bom-stm32node` today: build/flash (`task build`, `task flash`),
the USB CDC console (`version`, `stream`, `streamtest`, `radio`, `proto`), PDM
acquisition from D1 plus the PCM stream to the host (`stm32node-cli record`), and
RadioLib/SX1262 (`radio status|ping|reset`). The tests marked 🔧 still need:

- [ ] GNSS driver: UART4 NMEA parser + I2C access, `GPS_RESET`/`GPS_WAKE-UP`,
  EXTI on 1PPS, a `gps status` CLI command (fix, satellites, C/N0, TTFF)
- [ ] IMU driver: SPI3 + WHO_AM_I, accel/gyro reads, INT1/INT2, `imu` CLI command
- [ ] Magnetometer: I2C2 + WHO_AM_I, 3-axis reads, `mag` CLI command
- [ ] CLI control of the `EN_LORA` (PE7) and `EN_IMU` (PG14) load switches
- [ ] PDM: `LR_SEL` plus extending SAI to D2/D3 (multichannel) and a
  channel-alignment check
- [ ] ADC sampling of the analog microphones (timer + DMA, PA0/PA1)
- [ ] SD card: SPI5 driver + FatFs, `SD_DET`, a CLI write test and throughput
  measurement
- [ ] A `selftest` command that walks every peripheral and prints a PASS/FAIL
  table — so this checklist can be run on the next boards with one command

---

## 15. Test record

Board S/N: ................ Date: ................ Tested by: ................
Firmware commit: ................ Hardware revision: `bch-stmnode_v0.1`

| Section | Done | Pass | Fail (IDs) | Note / issue |
|---|---|---|---|---|
| 0. Before power-on | | | | |
| 1. Power | | | | |
| 2. MCU / clocks / console | | | | |
| 3. GNSS | | | | |
| 4. LoRa | | | | |
| 5. IMU | | | | |
| 6. Magnetometer | | | | |
| 7. PDM microphones | | | | |
| 8. Analog microphones | | | | |
| 9. microSD | | | | |
| 10. USB | | | | |
| 11. System tests | | | | |
| 12. Mechanics | | | | |

**Revision verdict:** ☐ board usable for firmware development ☐ usable with
limitations (list them) ☐ rework required before the next assembly
