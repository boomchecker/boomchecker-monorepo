# Toolchain

STM32 firmware is built **inside the `fw-devcontainer`**, the same container used for
ESP32 work — it ships the ARM bare-metal toolchain alongside ESP-IDF:

- `arm-none-eabi-gcc` + `libnewlib` — the bare-metal C compiler and standard library.
- **CMake + Ninja** — installed directly in the container (not dependent on sourcing
  ESP-IDF's `export.sh`).
- `openocd` and `st-flash` (stlink-tools) — flashing/debugging over an ST-Link probe.
- `gdb-multiarch` — source-level debugging.

No host-side STM32 install is needed; see
[Open the devcontainer](../../get-started/devcontainer.md).

Next: [Build & flash →](build.md)
