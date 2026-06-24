# Install Docker Desktop

Docker runs the devcontainers. On Windows, Docker Desktop uses the WSL2 engine you
just installed. Authoritative reference:
[Install Docker Desktop on Windows](https://docs.docker.com/desktop/install/windows-install/).

!!! info "License — free for us"
    Docker Desktop is **free** for personal use, education, and small businesses
    (< 250 employees **and** < $10M annual revenue). A paid subscription is only
    required in larger enterprises. University/thesis use is fine.

!!! warning "System requirements"
    WSL **2.1.5+** (`wsl --version`), Windows 10 22H2 (build 19045) or Windows 11 23H2
    (build 22631) or higher, a 64-bit CPU with SLAT, **8 GB RAM**, and **hardware
    virtualization enabled in BIOS/UEFI**.

## Check WSL is recent enough

In a terminal:

```powershell
wsl --version
```

If no version details appear (or it's below 2.1.5), update it — Docker Desktop needs a
modern WSL:

```powershell
wsl --update
```

## Install (Windows)

1. Download **Docker Desktop** from
   [docker.com](https://www.docker.com/products/docker-desktop/) (or the
   [Microsoft Store](https://apps.microsoft.com/detail/xp8cbj40xlbwkx)).
2. Run **Docker Desktop Installer.exe**. It asks for an installation mode:
    - **Per-user (recommended)** — installs to `%LOCALAPPDATA%`, **no admin rights**
      needed to install or update, WSL 2 backend only. Right for almost everyone.
    - **All users** — installs to `Program Files`, requires admin, supports Hyper-V and
      Windows containers (we don't need these).
3. On the configuration page, **keep "Use WSL 2 instead of Hyper-V" selected**.
4. Finish the wizard. Reboot if prompted.
5. Launch Docker Desktop, **Accept** the Subscription Service Agreement, and let it
   finish starting (the whale icon in the tray stops animating when ready).

!!! note "First-time WSL 2 enablement needs admin once"
    Even in per-user mode, enabling the WSL 2 feature the very first time is a one-time
    per-machine action that requires administrator privileges.

## Turn on the WSL2 backend

Open Docker Desktop → **Settings**:

1. **General** → make sure *Use the WSL 2 based engine* is checked.
2. **Resources → WSL Integration** → enable integration with your default distro
   (Ubuntu). This lets the `docker` command work from inside WSL.

Apply & restart.

## Verify

Open a terminal (PowerShell **or** your Ubuntu/WSL shell) and run:

```bash
docker run --rm hello-world
```

You should see a "Hello from Docker!" message. If you do, the engine works.

```bash
docker --version
docker compose version
```

!!! warning "Docker Desktop must be running"
    The devcontainer cannot start if Docker Desktop is not running. If VS Code says it
    can't reach Docker, check the tray icon first.

## Linux

Install **Docker Engine** (not Docker Desktop) following the
[official guide](https://docs.docker.com/engine/install/) for your distro, then add
yourself to the `docker` group so you don't need `sudo`:

```bash
sudo usermod -aG docker $USER
# log out and back in
```

## macOS

Install **Docker Desktop for Mac** from the same download page. The WSL settings above
don't apply; everything else (the devcontainer) works the same.

Next: [Install VS Code →](vscode.md)
