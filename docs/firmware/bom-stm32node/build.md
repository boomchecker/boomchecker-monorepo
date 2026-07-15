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
[usbipd](../../get-started/usbipd.md) on Windows), then either:

=== "st-flash (stlink-tools)"
    ```bash
    st-flash write build/bom-stm32node.bin 0x08000000
    ```

=== "OpenOCD"
    ```bash
    openocd -f interface/stlink.cfg -f target/stm32h5x.cfg \
      -c "program build/bom-stm32node.elf verify reset exit"
    ```

!!! note "MCU target"
    The board uses an **STM32H563ZIT6** (STM32H5 family), so OpenOCD uses
    `target/stm32h5x.cfg`. Flash base address is `0x08000000`.
