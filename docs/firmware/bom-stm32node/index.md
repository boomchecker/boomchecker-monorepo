# STM32 node — `bom-stm32node`

An alternative node platform built around the **STM32H563ZIT6**, configured with
**STM32CubeMX**. Source: `fw/bom-stm32node/` (the CubeMX project is
`bom-stm32node.ioc`).

The board integrates GNSS, LoRa, an IMU + magnetometer, and PDM microphones.

## In this section

- **[Toolchain](toolchain.md)** — what the `fw-devcontainer` provides for STM32
  (ARM compiler, CMake/Ninja, OpenOCD, st-flash).
- **[Build & flash](build.md)** — generate the CMake project from CubeMX, build it, and
  flash the board.
