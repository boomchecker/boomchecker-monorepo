# Firmware

Firmware lives under `fw/`. There are two node platforms:

| Project | MCU / framework | Status |
| ------- | --------------- | ------ |
| [`bom-node`](bom-node.md) | ESP32 / **ESP-IDF v5.4** | Primary node firmware |
| [`bom-stm32node`](bom-stm32node/index.md) | STM32H563ZIT6 / **STM32CubeMX** | Alternative node (GNSS, LoRa, IMU, PDM mics) |

## Building

All firmware is built inside the **`fw-devcontainer`**, which ships both the ESP-IDF
toolchain and an ARM bare-metal toolchain (`arm-none-eabi-gcc`, CMake/Ninja, OpenOCD,
st-flash) — see [Open the devcontainer](../get-started/devcontainer.md).

- **ESP32** (`bom-node`) — driven by Taskfile + `idf.py`. From `fw/bom-node`:

    ```bash
    task build      # idf.py build (also builds the embedded device-web UI)
    task flash      # idf.py flash
    task monitor    # idf.py monitor -p /dev/ttyUSB1
    ```

- **STM32** (`bom-stm32node`) — CubeMX project built with **CMake + Ninja**, flashed via
  OpenOCD / st-flash. See [Build & flash](bom-stm32node/build.md#build-via-cmake).

Flashing requires the USB device shared into the container — see
[Share USB with usbipd](../get-started/usbipd.md).
