# Start with the official ESP-IDF image
FROM espressif/idf:v5.4

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
    clang-format

# Add the default user to the dialout group for serial port access
RUN usermod -a -G dialout $(whoami) || true

# Install ESP debugging dependencies
RUN /bin/bash -c "source $IDF_PATH/export.sh && python -m pip install esp-debug-backend debugpy construct"

# Source the ESP-IDF export script to update PATH
RUN /bin/bash -c "source $IDF_PATH/export.sh"

# Install Node.js (LTS) and pnpm
RUN curl -fsSL https://deb.nodesource.com/setup_lts.x | bash - && \
    apt-get install -y nodejs && \
    npm install -g pnpm

# Set up PNPM storage to behave like on a local computer
ENV PNPM_HOME="/pnpm-store"
RUN mkdir -p $PNPM_HOME && \
    chown -R $(whoami) $PNPM_HOME && \
    echo "export PNPM_HOME=$PNPM_HOME" >> ~/.bashrc && \
    echo "export PATH=\$PNPM_HOME:\$PATH" >> ~/.bashrc

# Ensure ESP-IDF environment variables are sourced in Zsh
RUN echo "source /opt/esp/idf/export.sh > /dev/null 2>&1" >> ~/.bashrc

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

SHELL ["/bin/bash", "-c"]