# Toolchain

STM32 firmware is built **inside the `fw-devcontainer`**, the same container used for
ESP32 work — it ships the ARM bare-metal toolchain alongside ESP-IDF:

- `arm-none-eabi-gcc` + `libnewlib` — the bare-metal C compiler and standard library.
- **CMake + Ninja** — installed directly in the container (not dependent on sourcing
  ESP-IDF's `export.sh`).
- `st-flash` (stlink-tools) — flashing over an ST-Link probe (see [Build & flash](build.md)
  for why `openocd` doesn't work for this board out of the box).
- `gdb-multiarch` — source-level debugging.

No host-side STM32 install is needed; see
[Open the devcontainer](../../get-started/devcontainer.md).

Next: [Build & flash →](build.md)
