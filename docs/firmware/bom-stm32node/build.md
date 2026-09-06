# Build & flash

!!! warning "Configuring now needs the network"

    `cmake --preset Debug` clones CMSIS-DSP from github.com. The detector's
    arithmetic lives in `fw/common/boomdetect`, which fetches it at configure
    time rather than vendoring it - that is what removed 11 MB and 470 files
    from this repository, and the network dependency is the price. CI caches
    the fetched source. For an offline build, point
    `FETCHCONTENT_SOURCE_DIR_CMSISDSP` at a copy:

    ```sh
    cmake --preset Debug -DFETCHCONTENT_SOURCE_DIR_CMSISDSP=/path/to/CMSIS-DSP
    ```

## Build via CMake

The project is configured in **STM32CubeMX** (`bom-stm32node.ioc`). Generate the
project with the **CMake** toolchain option selected in CubeMX (Project Manager →
*Toolchain/IDE: CMake*). That produces a `CMakeLists.txt` and a
`cmake/gcc-arm-none-eabi.cmake` toolchain file.

!!! important "One-time prerequisite: the code-generation venv"
    This firmware `add_subdirectory()`s into `fw/common/boomlink`, which runs the
    Nanopb generator **on the build host** to turn `proto/*.proto` into the
    `*.pb.c`/`*.pb.h` the firmware links (see [BoomLink](boomlink.md)). That
    makes the host Python packages
    `protobuf` and `grpcio-tools` a hard requirement of building the firmware, not
    just of running BoomProtocol's own tests — without them CMake stops at configure
    time. Create them once, from the repository root:

    ```bash
    cd fw/common/boomlink && task setup
    ```

    `task build` in `fw/bom-stm32node` puts that venv on `PATH` automatically. If you
    prefer the raw `cmake` invocation below, activate it yourself first — from
    `fw/bom-stm32node`, that is `source ../common/boomlink/.venv/bin/activate` — or
    make sure your `python3` has both packages.

Then, from `fw/bom-stm32node`:

```bash
# Simplest path - handles the venv on PATH for you
task build
```

or with `cmake` directly:

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

## Two things that will cost you twenty minutes each

**The console is dead after flashing.** `openocd ... program ... reset exit`
leaves the USB device wedged: the board enumerates, but the CDC port neither
reads nor writes. It is not a crash — reading the fault registers over SWD shows
`HFSR = 0` and `CFSR = 0`. A separate reset fixes it:

```sh
openocd-stm32 -f interface/stlink-dap.cfg -c "transport select dapdirect_swd" \
  -f target/stm32h5x.cfg -c "init; reset halt; reset run; exit"
```

**`App/radio/*.cpp` will not compile in the fw devcontainer** as shipped:
`fatal error: cstring`. The image has `gcc-arm-none-eabi` but not the C++
standard library for the target, which is only an apt *Recommends*. CI installs
it explicitly; locally:

```sh
sudo apt-get install -y libstdc++-arm-none-eabi-newlib libnewlib-arm-none-eabi
```

## Talking to the board

The console is a USB CDC port. Use the `by-id` name rather than `/dev/ttyACMn` —
the number moves after a reflash, and one of the two is the ST-Link's own VCP:

```sh
picocom -b 115200 /dev/serial/by-id/usb-STMicroelectronics_boomchecker-node_*-if00
```

Exit with `Ctrl-A Ctrl-X`. Do not enable local echo; the board echoes already.
Note that `help` output is truncated: the console TX ring is 512 bytes
(`CLI_TX_RING` in `Core/Src/cli.c`) and the full help text is longer, so the tail
is silently dropped. This got worse, not better, with the detector: `model`,
`micslot` and `detselftest` are three more entries in the same buffer, and
`maxBindingCount` went to 24. Use `PROTOCOL.md` as the command reference until
the ring is resized; `help` is not a reliable way to check what an image
carries.
