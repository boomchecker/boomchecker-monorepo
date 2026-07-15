# Own image on Ubuntu 24.04 LTS: ESP32 (bom-node) + STM32 (bom-stm32node) firmware dev.
FROM ubuntu:24.04

# Avoid interactive tzdata/locales prompts during apt install on a bare Ubuntu image.
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

# Rename the image's built-in "ubuntu" user (UID/GID 1000) to "boom" instead of
# creating a new one — 1000 is the typical first-user UID on Linux/WSL hosts, so
# bind-mounted host files (/workspace) stay writable without a UID mismatch.
# If your host user's UID (`id -u`) isn't 1000, adjust here and in
# compose.devcontainer.yml's `user:` field to match.
RUN usermod -l boom -d /home/boom -m ubuntu && \
    groupmod -n boom ubuntu && \
    usermod -a -G dialout boom && \
    mkdir -p /home/boom/.ssh && \
    chown -R boom:boom /home/boom

# ESP-IDF build prerequisites (per Espressif's Linux setup docs) — the minimum needed
# to clone + install ESP-IDF below. Kept in its own layer, ahead of everything else,
# so later additions to the other apt blocks don't bust the cache on the slow
# ESP-IDF clone+install layer that follows.
RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    curl \
    wget \
    ca-certificates \
    flex \
    bison \
    gperf \
    python3 \
    python3-pip \
    python3-venv \
    cmake \
    ninja-build \
    ccache \
    libffi-dev \
    libssl-dev \
    dfu-util \
    libusb-1.0-0

# Install ESP-IDF v5.4 (matches the previous espressif/idf:v5.4 base image). This is
# the most expensive layer in the image (full clone + toolchain download) — placed
# right after its minimal prerequisites so it stays cached across unrelated changes
# further down the file.
ENV IDF_PATH=/opt/esp/idf
ENV IDF_TOOLS_PATH=/opt/esp
RUN git clone --recursive --shallow-submodules --depth 1 --branch v5.4 \
        https://github.com/espressif/esp-idf.git $IDF_PATH && \
    $IDF_PATH/install.sh esp32 && \
    chown -R boom:boom /opt/esp

# Install ESP debugging dependencies
RUN /bin/bash -c "source $IDF_PATH/export.sh && python -m pip install esp-debug-backend debugpy construct || { echo 'Warning: pip install for ESP debug deps failed (likely no network). Install inside the container when available.'; true; }"

# Source the ESP-IDF export script to update PATH
RUN /bin/bash -c "source $IDF_PATH/export.sh"

# Base tools + code-quality tooling + QEMU runtime deps (for `idf.py qemu`).
RUN apt-get update && apt-get install -y --no-install-recommends \
    gnupg \
    unzip \
    rsync \
    udev \
    xz-utils \
    libsdl2-2.0-0 \
    libgcrypt20 \
    libglib2.0-0 \
    libpixman-1-0 \
    libslirp0 \
    valgrind \
    clang \
    clang-tidy \
    clang-format

# STM32 platform support: ARM cross-toolchain + flashing/debugging tools.
# arm-none-eabi-gcc is the bare-metal compiler; OpenOCD / st-flash handle flashing
# over ST-Link. CMake + Ninja are installed above, independent of ESP-IDF.
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    libnewlib-arm-none-eabi \
    gdb-multiarch \
    openocd \
    stlink-tools

# Extra developer tools.
RUN apt-get update && apt-get install -y --no-install-recommends \
    doxygen \
    picocom \
    usbutils

# Install GitHub CLI from its official apt repo.
RUN mkdir -p -m 755 /etc/apt/keyrings && \
    wget -qO- https://cli.github.com/packages/githubcli-archive-keyring.gpg | tee /etc/apt/keyrings/githubcli-archive-keyring.gpg > /dev/null && \
    chmod go+r /etc/apt/keyrings/githubcli-archive-keyring.gpg && \
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | tee /etc/apt/sources.list.d/github-cli.list > /dev/null && \
    apt-get update && apt-get install -y --no-install-recommends gh && \
    rm -rf /var/lib/apt/lists/*

# Install Node.js LTS (v20) from NodeSource
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs && \
    rm -rf /var/lib/apt/lists/*

# Install pnpm
RUN npm install -g pnpm

# Install Taskfile
RUN curl --location https://taskfile.dev/install.sh | sh -s -- -b /usr/local/bin

# Install Changesets CLI
RUN npm install -g @changesets/cli

# AI tools: Codex CLI + Claude Code CLI
RUN npm install -g @openai/codex @anthropic-ai/claude-code

# Prepare Codex/Claude config dirs so the mounted volumes keep correct ownership
RUN mkdir -p /home/boom/.codex /home/boom/.claude && \
    chown -R boom:boom /home/boom/.codex /home/boom/.claude

# Set up PNPM storage to behave like on a local computer
ENV PNPM_HOME="/pnpm-store"
RUN mkdir -p $PNPM_HOME && \
    echo "export PNPM_HOME=$PNPM_HOME" >> ~/.bashrc && \
    echo "export PATH=\$PNPM_HOME:\$PATH" >> ~/.bashrc && \
    chown -R boom:boom $PNPM_HOME

# Ensure ESP-IDF environment variables are sourced in Bash
RUN echo "source $IDF_PATH/export.sh > /dev/null 2>&1" >> ~/.bashrc

# Configure boom user with ESP-IDF environment
RUN echo "source $IDF_PATH/export.sh > /dev/null 2>&1" >> /home/boom/.bashrc && \
    echo "export PNPM_HOME=/pnpm-store" >> /home/boom/.bashrc && \
    echo "export PATH=\$PNPM_HOME:\$PATH" >> /home/boom/.bashrc && \
    chown -R boom:boom /home/boom/.bashrc

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

SHELL ["/bin/bash", "-c"]
