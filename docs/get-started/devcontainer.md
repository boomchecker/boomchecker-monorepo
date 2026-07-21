# Open the devcontainer

This is the payoff. With WSL, Docker, and VS Code in place, you open the project inside
a ready-made environment — no toolchain install required.

## Choose your container

The repo ships **two** devcontainers under `.devcontainer/`. Pick by what you work on:

=== "Firmware → `fw-devcontainer`"
    For firmware in `fw/` — both **ESP32** and **STM32** platforms.

    - Base image: **`ubuntu:24.04`**, with ESP-IDF 5.4 installed directly (cloned
      + `install.sh`) and **CMake + Ninja** installed as system packages.
    - **ESP32** (`bom-node`): the full ESP-IDF toolchain (`idf.py`).
    - **STM32** (`bom-stm32node`): an ARM bare-metal toolchain —
      `arm-none-eabi-gcc`, `libnewlib`, `gdb-multiarch` — plus `openocd` and
      `st-flash` (stlink-tools) for flashing over ST-Link. STM32CubeMX-generated
      projects build with **CMake + Ninja**, independent of ESP-IDF.
    - Also includes: Node.js 20, pnpm, [Taskfile](https://taskfile.dev), Changesets,
      `clang`/`clang-tidy`/`clang-format`, `doxygen`, `gh`, `picocom`, `usbutils`,
      QEMU dependencies, and `valgrind`.
    - Runs **privileged** with `SYS_PTRACE` so debugging and USB flashing work.
    - VS Code extensions: ESP-IDF, C/C++, CMake Tools, Docker, GitHub PR, Prettier.

=== "Software → `sw-devcontainer`"
    For the apps in `apps/`, the Python/C scripts in `scripts/`, and these docs.

    - Base image: **`golang:1.24-bookworm`** (Go 1.24 + Debian, Python 3.11).
    - Also includes: Node.js 20, pnpm, Taskfile, Changesets, Go tooling
      (`delve`, `gopls`, `golangci-lint`, `staticcheck`), `clang-format`, `cmake`,
      `valgrind`.
    - Shell is **zsh** with oh-my-zsh.
    - VS Code extensions: Go, Docker, GitHub PR, Prettier, C/C++.

Both are defined through `.devcontainer/compose.devcontainer.yml`, mount the repo at
`/workspace`, and run as a non-root user (**`boom`** for `fw-devcontainer`, **`dev`** for
`sw-devcontainer`). Your host `~/.ssh` is mounted read-only; on first create, a
`postCreateCommand` copies its keys into a persistent `ssh-data` volume (owned by the
container user, with correct `600`/`644` permissions) so git over SSH works inside the
container without mutating your host's SSH directory.

## Open it

1. **Get the code and open the folder** in VS Code:

    ```bash
    git clone <repo-url> ~/boomchecker-monorepo
    cd ~/boomchecker-monorepo
    code .
    ```

    (Alternatively, let VS Code clone straight into a fast Docker volume — see the
    Command Palette step below and run *Dev Containers: Clone Repository in Container
    Volume…*.)

2. **Open the Command Palette.** This is VS Code's command launcher. Open it by:
    - clicking the **search bar at the top** of the window and typing **`>`**, or
    - pressing **++f1++**, or **++ctrl+shift+p++** (**++cmd+shift+p++** on macOS).

3. Type **`Dev Containers`** and you'll see the relevant commands:
    - **Dev Containers: Reopen in Container** — reopen using the **already-built**
      container. Fast. Use this normally.
    - **Dev Containers: Rebuild and Reopen in Container** — rebuild the image from
      scratch, then reopen. Use this the **first time**, and whenever the devcontainer
      definition changes.

4. You're asked **which** devcontainer config to use — pick `fw` or `sw` (see the table
   above).

!!! warning "Rebuilds can take a while"
    A **rebuild** re-runs the whole image build. For the **`fw-devcontainer`**, the
    first build can take a **while** — ESP-IDF and the STM32 toolchain are cloned and
    installed from scratch on plain Ubuntu, not pulled from a prebuilt vendor image. Let
    it finish. A plain *Reopen* afterwards is near-instant.

!!! tip "When do I need to rebuild?"
    Any change under `.devcontainer/` (Dockerfile, `compose.devcontainer.yml`,
    `devcontainer.json`) only takes effect after **Rebuild and Reopen in Container**. If
    you just pulled changes that touched the devcontainer, rebuild. Otherwise a normal
    *Reopen* is enough.

!!! tip "Switch containers any time"
    Working on firmware *and* an app? Reopen in the other container via the Command
    Palette → *Dev Containers: Reopen in Container* and choose the other config. You can
    also run two VS Code windows side by side.

## After it opens

You now have a terminal **inside** the container. Bootstrap the workspace and discover
the available tasks:

```bash
task setup          # installs JS dependencies for the workspace (pnpm install)
task -l             # list every available task in the repo
```

`task -l` is your map — it prints every build/flash/test/docs task. Some areas have
their own extra setup (e.g. the Python scripts under `scripts/` have a `setup` task that
builds a virtualenv) — those show up in `task -l` too.

!!! success "How do I know it worked?"
    Inside the container terminal, the toolchain is on the PATH. Try `idf.py --version`
    in the `fw` container, or `go version` in the `sw` container. If they print
    versions, you're in.

Next:

- Flashing real hardware? → [Share USB with usbipd](usbipd.md)
- Otherwise → [Your first build](first-build.md)
