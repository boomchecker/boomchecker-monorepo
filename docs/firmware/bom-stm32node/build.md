# Build & flash

## Build via CMake

The project is configured in **STM32CubeMX** (`bom-stm32node.ioc`). Generate the
project with the **CMake** toolchain option selected in CubeMX (Project Manager →
*Toolchain/IDE: CMake*). That produces a `CMakeLists.txt` and a
`cmake/gcc-arm-none-eabi.cmake` toolchain file. Then, from `fw/bom-stm32node`:

```bash
# Configure (Ninja generator, ARM cross toolchain from CubeMX)
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=Debug

# Build → produces build/bom-stm32node.elf (+ .bin / .hex)
cmake --build build
```

## Flash

Attach the ST-Link (share the USB device into the container with
[usbipd](../../get-started/usbipd.md) on Windows), then use **st-flash**
(stlink-tools) — the devcontainer's `task flash` (in `fw/bom-stm32node/`) does exactly
this:

```bash
arm-none-eabi-objcopy -O binary build/bom-stm32node.elf build/bom-stm32node.bin
st-flash write build/bom-stm32node.bin 0x08000000
```

!!! note "MCU target"
    The board uses an **STM32H563ZIT6** (STM32H5 family). Flash base address is
    `0x08000000`. `stlink-tools` 1.8.0+ (the version in the devcontainer) knows this
    chip; older versions may not.

!!! warning "OpenOCD doesn't work out of the box here"
    Neither the devcontainer's apt-installed OpenOCD (0.12.0, Ubuntu package) nor the
    ESP-IDF-bundled OpenOCD fork (ahead on `PATH` once `export.sh` is sourced) ships a
    `target/stm32h5x.cfg` — STM32H5 support post-dates both. Use `st-flash` instead, or
    build/install a newer upstream OpenOCD yourself if you need SWD debugging via
    OpenOCD specifically.
