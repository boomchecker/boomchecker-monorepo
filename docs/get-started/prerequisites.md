# Prerequisites

Before installing anything, line up the accounts, access, and hardware you will need.

## Accounts and access

- **A GitHub account.** Use a real name you are comfortable with; this is where your
  thesis work will live.
- **Membership in the Boomchecker GitHub organization.** Ask Martin Maxa
  (`maxamart@fel.cvut.cz`) to add you to the organization and grant access to the
  monorepo. You cannot push branches until you are a member — see
  [Contributing](contributing.md) for the branch + PR workflow.
- **Git installed** (covered later inside the devcontainer, but you also want it on
  the host to clone the repo). On Windows it comes with [Git for Windows](https://git-scm.com/download/win).

## Hardware

| What | Why |
| ---- | --- |
| A reasonably modern laptop/PC (8 GB RAM minimum, 16 GB comfortable) | Docker + the toolchain are not tiny. |
| **Windows 11**, or Windows 10 version 2004+ (build 19041+) | Required for WSL2. Check with `winver`. |
| Admin rights on the machine | WSL2, Docker Desktop, and usbipd all need it to install. |
| An ESP32 / STM32 dev board + USB cable | Only if you do **firmware** work and want to flash real hardware. |

!!! warning "Use a data cable, not a charge-only cable"
    Many USB cables only carry power. If your board never shows up as a serial port,
    swap the cable before debugging anything else.

## Operating system notes

=== "Windows"
    This is the primary, fully documented path. Continue to [Install WSL2](wsl.md).

=== "Linux"
    You do **not** need WSL or usbipd. Install Docker Engine and VS Code natively, then
    jump to [Open the devcontainer](devcontainer.md). USB devices are already visible
    to Docker via `/dev`, so flashing just works.

=== "macOS"
    Install Docker Desktop for Mac and VS Code, then go to
    [Open the devcontainer](devcontainer.md). Note: **USB serial passthrough into a
    Linux container on macOS is not reliable** — for flashing real hardware, prefer a
    Windows or Linux machine. App/script development works fine.

## What you do *not* install

You will **not** install ESP-IDF, Go, Node.js, Python, pnpm, or Doxygen on your host
machine. All of that lives inside the devcontainer. Resist the urge — installing them
by hand is exactly what the devcontainer exists to avoid.

Next: [Install WSL2 →](wsl.md)
