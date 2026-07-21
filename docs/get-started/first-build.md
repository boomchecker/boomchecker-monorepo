# Your first build

You're inside the devcontainer with `task -l` working. Time to actually build
something. Pick the track that matches your work.

## Recap of the setup so far

From the project README, the short version of everything you've done:

1. Install Docker Desktop with the WSL2 backend.
2. Open the repo in VS Code and launch the appropriate devcontainer (`fw` or `sw`).
3. Run `pnpm install` inside the container if a project needs it.
4. Run the project's setup task and start coding.

Use `task -l` whenever you're unsure which tasks exist.

## Track A — Firmware (ESP32, `fw-devcontainer`)

The ESP32 node lives in `fw/bom-node` and is driven by [Taskfile](https://taskfile.dev).

!!! warning "Flashing needs a shared USB port"
    Make sure you attached the board with [usbipd](usbipd.md) and see a `/dev/ttyUSB*`
    or `/dev/ttyACM*` device first.

From the firmware directory:

```bash
cd fw/bom-node

task build      # or: task b  — compiles the firmware (runs idf.py build)
task flash      # or: task f  — flashes it onto the board (idf.py flash)
task monitor    # or: task m  — opens the serial monitor (idf.py monitor)
```

What to expect:

- `task build` first builds the **device-web** UI as a dependency and embeds it, then
  compiles the firmware. The first build is slow; later builds are incremental.
- `task monitor` defaults to `/dev/ttyUSB1`. If your board enumerated as a different
  port, call the underlying tool directly, e.g.:

    ```bash
    idf.py -p /dev/ttyUSB0 flash monitor
    ```

- Exit the monitor with ++ctrl+right-bracket++ (`Ctrl+]`).

??? note "Doing it without Taskfile"
    The tasks are thin wrappers over standard ESP-IDF commands: `idf.py build`,
    `idf.py flash`, `idf.py monitor`. Anything the ESP-IDF docs say works here.

## Track B — Apps (`sw-devcontainer`)

```bash
task setup           # once, installs workspace dependencies (pnpm install)
task -l              # see app tasks (namespaced, e.g. api:* bot:* )
```

A few entry points:

- **`api-backend`** (Go REST API) — see `apps/api-backend/README.md` and the
  [Apps](../apps/index.md) page.
- **`boom-discord-bot`** (Cloudflare Worker) — `task bot:dev` runs it locally,
  `task bot:test` runs unit tests, `task bot:typecheck` type-checks.
- **`device-web`** (local UI) — `task run` inside `apps/device-web` serves the UI.

## Track C — Scripts (`sw-devcontainer`)

The DSP experiments under `scripts/` each have their own `setup` task that creates a
Python virtualenv:

```bash
cd scripts/median-filter
task setup          # installs Python deps + ffmpeg
task benchmark      # builds the C core and benchmarks it
```

See the [Scripts](../scripts/index.md) page for the peak detector, FxLMS, and TDOA
tools.

## Build the docs locally

This very site builds with one command in the `sw-devcontainer`:

```bash
task docs:serve     # live preview at http://localhost:8000
task docs           # one-off build into ./site
```

!!! success "Milestone"
    If you flashed a board, or ran an app, or previewed the docs — your environment is
    fully working. Now learn how to contribute your changes.

Next: [Git basics →](git-basics.md)
