# Install VS Code

We develop inside **Visual Studio Code** because it can open a project *inside* a
container transparently — you edit, build, and debug as if the toolchain were local,
but it all runs in the devcontainer.

## Install

1. Download **VS Code** from [code.visualstudio.com](https://code.visualstudio.com/)
   and install it on **Windows** (not inside WSL — the Windows app connects into WSL
   and containers for you).
2. Launch it.

## Install the Dev Containers extension

1. Open the **Extensions** panel (`Ctrl+Shift+X`).
2. Search for and install **Dev Containers** (publisher: Microsoft, id
   `ms-vscode-remote.remote-containers`).
3. While you're there, the **WSL** extension (`ms-vscode-remote.remote-wsl`) is also
   handy.

!!! info "You don't install the project extensions yourself"
    The devcontainers declare the extensions they need (ESP-IDF, C/C++, CMake, Go,
    Prettier, GitHub Pull Requests, …). VS Code installs those *into the container*
    automatically when it opens. You only need the **Dev Containers** extension on the
    host.

## Verify

Press `F1` (or `Ctrl+Shift+P`) to open the command palette and type
`Dev Containers`. If you see commands like **"Dev Containers: Reopen in Container"**,
the extension is ready.

## Recommended host setup

- Sign in to your **GitHub account** in VS Code (Accounts icon, bottom-left). This makes
  cloning private repos and using the Pull Requests panel painless.
- Set your git identity once (covered in [Git basics](git-basics.md)).

Next: [Open the devcontainer →](devcontainer.md)
