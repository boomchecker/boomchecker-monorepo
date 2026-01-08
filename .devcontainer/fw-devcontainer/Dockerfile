# Start with the official ESP-IDF image
FROM espressif/idf:v5.4

# Create dev user with UID 1001 (avoid conflicts)
RUN useradd -m -u 1001 -s /bin/bash dev && \
    usermod -a -G dialout dev && \
    mkdir -p /home/dev/.ssh && \
    chown -R dev:dev /home/dev

# Install dependencies required for QEMU, Zsh, and Oh My Zsh
RUN apt update && apt install -y \
    wget \
    xz-utils \
    git \
    curl \
    libsdl2-2.0-0 \
    libgcrypt20 \
    libglib2.0-0 \
    libpixman-1-0 \
    libslirp0 \
    coreutils \
    udev \
    valgrind \
    clang \
    clang-tidy \
    clang-format \
    ca-certificates

# Install Node.js LTS (v20) from NodeSource
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs && \
    rm -rf /var/lib/apt/lists/*

# Add the default user to the dialout group for serial port access
RUN usermod -a -G dialout $(whoami) || true

# Install ESP debugging dependencies
RUN /bin/bash -c "source $IDF_PATH/export.sh && python -m pip install esp-debug-backend debugpy construct || { echo 'Warning: pip install for ESP debug deps failed (likely no network). Install inside the container when available.'; true; }"

# Source the ESP-IDF export script to update PATH
RUN /bin/bash -c "source $IDF_PATH/export.sh"

# Install pnpm
RUN npm install -g pnpm

# Install Taskfile
RUN curl --location https://taskfile.dev/install.sh | sh -s -- -b /usr/local/bin

# Install Changesets CLI
RUN npm install -g @changesets/cli

# Set up PNPM storage to behave like on a local computer
ENV PNPM_HOME="/pnpm-store"
RUN mkdir -p $PNPM_HOME && \
    chown -R $(whoami) $PNPM_HOME && \
    echo "export PNPM_HOME=$PNPM_HOME" >> ~/.bashrc && \
    echo "export PATH=\$PNPM_HOME:\$PATH" >> ~/.bashrc

# Ensure ESP-IDF environment variables are sourced in Bash
RUN echo "source /opt/esp/idf/export.sh > /dev/null 2>&1" >> ~/.bashrc

# Configure dev user with ESP-IDF environment
RUN echo "source /opt/esp/idf/export.sh > /dev/null 2>&1" >> /home/dev/.bashrc && \
    echo "export PNPM_HOME=/pnpm-store" >> /home/dev/.bashrc && \
    echo "export PATH=\$PNPM_HOME:\$PATH" >> /home/dev/.bashrc && \
    chown -R dev:dev /home/dev/.bashrc

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

SHELL ["/bin/bash", "-c"]
