# ESP32 node — `bom-node`

The primary node firmware, built on **ESP-IDF v5.4** for the ESP32. Source:
`fw/bom-node/`.

## What it does

`bom-node` runs the acoustic detection node: it samples audio, runs detection, and
serves a small local web UI (the **device-web** app) that is built and embedded into the
firmware image at build time.

## Build, flash, monitor

Inside the `fw-devcontainer`, from `fw/bom-node`:

```bash
task build      # or task b — runs idf.py build, embeds the device-web UI
task flash      # or task f — runs idf.py flash
task monitor    # or task m — runs idf.py monitor -p /dev/ttyUSB1
```

If your board enumerated on a different serial port, call ESP-IDF directly:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

See [usbipd](../get-started/usbipd.md) for getting the serial port into the container.

## Configuration

Firmware configuration is the standard ESP-IDF setup:

- `fw/bom-node/CMakeLists.txt` — project build config.
- `fw/bom-node/sdkconfig` — ESP-IDF / Kconfig settings (`idf.py menuconfig` to edit).

## Embedded web UI

`task build` depends on `device-web:build`, which copies the local UI from
`apps/device-web/public/` into `fw/bom-node/generated/` so it gets flashed with the
firmware. See [Device web](../apps/device-web.md).
