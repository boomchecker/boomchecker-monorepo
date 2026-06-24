# Share USB with usbipd

WSL2 (and therefore the devcontainer) cannot see USB devices on Windows by default. To
flash or monitor an ESP32/STM32 board, you **forward** its USB serial port from Windows
into WSL using **usbipd-win**.

!!! info "Who needs this"
    Only **firmware** work that talks to real hardware. App/script developers and
    anyone on **native Linux** can skip this page — Linux exposes USB to containers
    directly.

## Install usbipd-win

On the **Windows** side (PowerShell as Administrator), either:

```powershell
winget install usbipd
```

…or grab the installer from the
[releases page](https://github.com/dorssel/usbipd-win/releases) (v4.x or newer; the
docs were written against v5.x).

Restart your terminal afterwards so `usbipd` is on the PATH.

## Attach the board — every time you plug it in

1. Plug the board into USB.
2. List devices (Administrator PowerShell):

    ```powershell
    usbipd list
    ```

    Find your board — typically a *USB Serial*, *CP210x*, *CH340*, or *USB JTAG/serial*
    device — and note its **BUSID** (e.g. `2-4`).
3. The **first time only**, bind it (one-off, persists across reboots):

    ```powershell
    usbipd bind --busid 2-4
    ```
4. Attach it to WSL (repeat after every replug / reboot):

    ```powershell
    usbipd attach --wsl --busid 2-4
    ```

## Verify inside WSL / the container

In your WSL or devcontainer terminal:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
dmesg | tail
```

You should see a new `/dev/ttyUSB0` (CP210x/CH340) or `/dev/ttyACM0` (native USB JTAG).
The firmware tasks expect a serial port here.

!!! note "Which port?"
    The firmware monitor task is wired to `/dev/ttyUSB1` by default. If your board shows
    up as a different port, pass it explicitly (e.g.
    `idf.py -p /dev/ttyUSB0 monitor`) — see [Your first build](first-build.md).

## Detach

When you're done (or to give the port back to Windows):

```powershell
usbipd detach --busid 2-4
```

## Troubleshooting

- **Device not in `usbipd list`** → bad cable (charge-only) or no driver. Try another
  cable/port.
- **`attach` fails** → run `wsl --update`, make sure a WSL distro is running, and that
  you `bind` before `attach`.
- **Port disappears after replug** → re-run `usbipd attach`. The attach does not
  survive unplugging the device.
- **Permission denied on `/dev/ttyUSB0`** → the container runs privileged so this is
  rare; if it happens, reopen the container.

Next: [Your first build →](first-build.md)
