# Boomchecker

Boomchecker is a research and development monorepo for **acoustic detection units** —
small embedded devices that listen for impulse-like events, locate them, and report
them. The repository holds everything: firmware, backend services, signal-processing
experiments, hardware design, and this documentation.

This site has two jobs:

1. **Get a newcomer productive.** If you have never touched this project — or never
   used git, WSL, or a devcontainer — start at **[Get Started](get-started/index.md)**
   and read it top to bottom. By the end you will have flashed a device or run a
   service on your own machine.
2. **Document the components.** Reference docs for the [firmware](firmware/index.md),
   the [apps](apps/index.md), and the [scripts](scripts/index.md) live under their own
   tabs.

## Who this is for

This monorepo is intentionally **decentralized**: bachelor's and master's students do
their thesis work directly inside it. You do not need to be an embedded or web expert
to start. You *do* need to follow the shared rules so the repository stays clean for
everyone — those rules are in [Monorepo rules](get-started/monorepo-rules.md) and
[Contributing](get-started/contributing.md).

## The 60-second tour

| Directory        | What lives there                                                    |
| ---------------- | ------------------------------------------------------------------- |
| `apps/`          | Backend (`api-backend`, Go) and services (`boom-discord-bot`, `device-web`) |
| `fw/`            | Firmware: `bom-node` (ESP32 / ESP-IDF), `bom-stm32node` (STM32)     |
| `scripts/`       | DSP and analysis tools (peak detector, FxLMS, TDOA)                 |
| `hw/`            | Hardware: Altium libraries, templates, and node schematics          |
| `templates/`     | Document templates (IEEE conference paper)                          |
| `.devcontainer/` | Docker dev environments for VS Code (`fw` and `sw`)                 |
| `docs/`          | This documentation (MkDocs Material)                                |

!!! tip "First time here?"
    Go straight to **[Get Started → Overview](get-started/index.md)**. Everything you
    need to set up your machine is there, in order.
