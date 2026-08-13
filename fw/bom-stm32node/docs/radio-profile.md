# SX1262 bring-up LoRa PHY profile

This documents the fixed profile `App/radio/e22_radio.cpp` (`e22_radio::DefaultProfile()`)
configures on every boot, and why it is expected to comply with the Czech/EU 869.4-869.65 MHz
sub-band limits described in
[`docs/firmware/bom-stm32node/boomlink.md`](../../../docs/firmware/bom-stm32node/boomlink.md)
section 6.1. It is the **PR1 bring-up profile only** - proving raw two-board RF
communication - not a final field/deployment profile; that choice belongs to the runtime
`RadioConfig` added in PR4.

## Profile

| Parameter          | Value                          |
|--------------------|---------------------------------|
| Carrier frequency  | 869.525 MHz                    |
| Bandwidth          | 125 kHz                        |
| Spreading factor   | SF7                             |
| Coding rate        | 4/5                              |
| TX power           | 14 dBm (conducted)              |
| Preamble           | 8 symbols                        |
| Sync word          | 0x12 (`RADIOLIB_SX126X_SYNC_WORD_PRIVATE`, not the 0x34 LoRaWAN/public word) |
| TCXO reference     | 1.8 V on DIO3                    |
| Regulator          | DC-DC (`useRegulatorLDO = false`, SX1262 reference default) |

SF7 was picked over SF9 for PR1 specifically because bring-up is a bench/development
activity (two boards, short range, fast iteration on `radio ping`): lower airtime per
packet means faster feedback and less duty-cycle budget burned per test. SF7-SF9 are all
expected to be adequate for the deployed range per boomlink.md section 1.1; the field
default is a later decision.

## Band-plan compliance (ERC 70-03, 869.4-869.65 MHz sub-band)

That sub-band allows up to **500 mW ERP** and **10% duty cycle**, the most permissive of
the 863-870 MHz SRD allocations.

- **In-band**: centre frequency 869.525 MHz is the exact midpoint of 869.4-869.65 MHz.
  At 125 kHz occupied bandwidth the signal spans 869.4625-869.5875 MHz, i.e. 62.5 kHz of
  margin to each sub-band edge.
- **Power - margin not actually established, needs hardware verification**: the profile
  sets `SX1262::begin()`'s `power` parameter to 14 dBm, which programs the **SX1262 die's
  own output register**, not the module's antenna-port power. The E22-900M22S is rated up
  to 22 dBm and, per EBYTE's own product line, achieves that with an **integrated PA**
  stage after the SX1262 - so the actual radiated power for a given die setting depends on
  that PA's gain, which this document does not have a verified figure for (not reachable
  from this sandboxed session; no hardware to measure it either). The "13 dB below the
  27 dBm ERP ceiling" claim in an earlier version of this document assumed die output ==
  antenna-port output, which does not hold for a PA-integrated module and should not be
  trusted until confirmed. **Before first power-up, get the actual small-signal gain of
  the E22-900M22S's PA stage from EBYTE's datasheet and recompute the real margin against
  the 500 mW/27 dBm ERP ceiling** - it may require a materially lower `power` setting than
  14 dBm. PR1 exposes no runtime power control (the profile is fixed at compile time); a
  later PR's `RadioConfig` (boomlink.md section 8.2) is where a legal-power ceiling needs
  to be enforced against operator-supplied values, using whatever die-to-antenna
  relationship is confirmed here.
- **Duty cycle**: PR1 does not implement automatic duty-cycle enforcement (boomlink.md
  section 6.1 explicitly defers this: "automatic duty-cycle enforcement may be added later
  if measurements show it is needed at this scale"). `radio ping` during interactive
  bring-up testing is inherently low-duty (operator-paced CLI commands), well under 10%,
  but nothing currently prevents software from transmitting more aggressively. A
  cumulative TX airtime counter (boomlink.md section 9.10) is BoomLink-layer scope (PR3+),
  not implemented here.

## TCXO voltage - please verify before first power-up

The EBYTE E22-900M22S integrates a 32 MHz TCXO powered from the SX1262's DIO3 pin
(boomlink.md section 6). RadioLib must be told the exact reference voltage that TCXO
expects, via `SX1262::begin()`'s `tcxoVoltage` parameter (wired to `setTCXO()`
internally) - **an incorrect value prevents the oscillator from locking and the radio
will not transmit or receive on frequency at all**, with no other symptom.

This profile uses **1.8 V**, based on community-reported working configurations for the
E22-900M22S/E22-900M30S family (e.g.
<https://github.com/jgromes/RadioLib/discussions/487>), since the vendor datasheet was
not reachable from this sandboxed session to confirm directly. **Before first power-up
on real hardware, cross-check 1.8 V against the EBYTE E22-900M22S datasheet for the
exact PCB revision in use** and adjust `e22_radio::DefaultProfile()` if it differs.

## Where this is implemented

- `App/radio/e22_radio.h` / `.cpp` - the `Profile` struct and `DefaultProfile()`.
- `App/radio/radio.cpp` - passes the profile straight into `SX1262::begin()`.
- `radio status` (CLI) - prints the active profile and live link stats (TX/RX counts,
  last RSSI/SNR) for verifying the link on the bench.
