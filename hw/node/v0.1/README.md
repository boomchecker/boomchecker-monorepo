# BCH Node Hardware v0.1

Firmware-facing hardware notes for the `bch-node_v0.1` board revision. This
document is intended as an engineering handoff for STM32/CubeMX setup and
firmware bring-up, not as a replacement for the Altium project or component
datasheets.

Bring-up testy pro první osazenou revizi jsou v
[`bringup-checklist.md`](bringup-checklist.md).

## Source Status

- Hardware project: `bch-node_v0.1.PrjPcb`
- MCU: `STM32H563ZIT6`
- Primary schematic sheets: `top.SchDoc`, `mcu.SchDoc`, `gps.SchDoc`,
  `lora.SchDoc`, `imu.SchDoc`, `power.SchDoc`
- Existing CubeMX project: `fw/bom-stm32node/bom-stm32node.ioc`

The current `.ioc` file is treated as a firmware draft only. It must not be
used as the source of truth until reconciled against the Altium compiled
schematic or a verified pin/net export.

## Interface Summary

| Subsystem | Main component | Firmware interface | Status |
| --- | --- | --- | --- |
| MCU | `STM32H563ZIT6` | GPIO, SPI, I2C, USB, SAI/PDM | Pin map incomplete |
| GNSS | `Teseo-LIV3R` | I2C, GPIO, PPS interrupt | Harness verified, MCU pins TBD |
| LoRa | `E22-900M22S` | SPI, GPIO, IRQ/status | Harness verified, MCU pins TBD |
| IMU accel/gyro | `ISM330DLCTR` | SPI, GPIO interrupts | Device verified, MCU pins TBD |
| Magnetometer | `LIS2MDLTR` | I2C2 (`MAG_SCL`/`MAG_SDA`) | Device verified, MCU pins TBD |
| PDM microphones | `IM67D130AXTSA2` | SAI/PDM clock and data | Design intent, schematic/CubeMX TBD |
| Power | USB-C, `BQ21040DBVR`, `TPS563252DRLR` | No MCU control currently planned | Out of pin-map scope |