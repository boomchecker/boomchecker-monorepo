# Install WSL2

**WSL2** (Windows Subsystem for Linux, version 2) runs a real Linux kernel inside
Windows — Linux apps and Bash tools run directly on Windows, without a full virtual
machine or dual-boot. Docker Desktop uses it as its engine, and your USB devices are
forwarded into it. You need it before Docker.

This page is the short version; the authoritative reference is Microsoft's
[Install WSL guide](https://learn.microsoft.com/windows/wsl/install).

!!! info "Linux / macOS users"
    Skip this page entirely. WSL is a Windows-only concept.

!!! warning "Windows version"
    You need **Windows 11**, or **Windows 10 version 2004+ (build 19041+)**. Check with
    `winver`. On older builds, follow Microsoft's
    [manual installation steps](https://learn.microsoft.com/windows/wsl/install-manual).

## Install

1. Open **PowerShell** or **Windows Terminal as Administrator** (right-click → *Run as
   administrator*).
2. Run:

    ```powershell
    wsl --install
    ```

    This enables the required Windows features, installs WSL2, and installs **Ubuntu**
    by default. To pick a different distribution, list and choose one:

    ```powershell
    wsl --list --online            # see available distros
    wsl --install -d Debian        # install a specific one
    ```
3. **Reboot** when prompted.
4. After reboot, the distribution launches, decompresses (first run only), and asks you
   to create a **UNIX username and password**. Pick something simple and remember it —
   you will use that password for `sudo` inside Linux.

!!! note "`wsl --install` prints help text instead of installing?"
    That means WSL is already partly present. Install a distro explicitly with
    `wsl --install -d <Distro>`. If the install **hangs at 0.0%**, force a download
    first: `wsl --install --web-download -d <Distro>`.

## Verify

Back in PowerShell:

```powershell
wsl --status
wsl --list --verbose
```

You want to see your distro listed with **VERSION 2**. If it says VERSION 1, convert it:

```powershell
wsl --set-version Ubuntu 2
wsl --set-default-version 2
```

Also make sure WSL is up to date (this matters for usbipd later):

```powershell
wsl --update
```

## A few orientation tips

- Launch Linux any time by typing `wsl` in PowerShell, or opening your distro (e.g.
  **Ubuntu**) from the Start menu. **[Windows Terminal](https://learn.microsoft.com/windows/terminal/)**
  is the recommended way to run it — tabs for WSL, PowerShell, etc.
- `wsl --set-default <Distro>` picks which distro `wsl` opens by default.
- Your Windows drives are mounted under `/mnt/c`, `/mnt/d`, etc.
- **Keep your code on the Linux side, not under `/mnt/c`.** File access across the
  Windows/Linux boundary is slow. Clone the repo into your Linux home (`~/`), or — what
  we actually do — let VS Code clone it into the container's volume. More on that in
  [Open the devcontainer](devcontainer.md).

!!! tip "How much RAM does WSL use?"
    WSL2 grabs memory dynamically and can hold onto it. If you want to cap it, create
    `C:\Users\<you>\.wslconfig` with:

    ```ini
    [wsl2]
    memory=8GB
    processors=4
    ```

    Then run `wsl --shutdown` and reopen.

Next: [Install Docker Desktop →](docker.md)
