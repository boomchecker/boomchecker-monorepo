# Start with the official Go image on Debian bookworm (has Python 3.11)
FROM golang:1.24-bookworm

# Install basic development tools including Python 3.11
RUN apt-get update && apt-get install -y \
    git \
    curl \
    wget \
    make \
    build-essential \
    ca-certificates \
    python3 \
    python3-pip \
    python3-venv \
    doxygen \
    graphviz \
    zsh \
    clang-format \
    cmake \
    valgrind \
    sudo \
    && rm -rf /var/lib/apt/lists/*

# Ensure `python` and `pip` commands are available
RUN ln -sf /usr/bin/python3 /usr/local/bin/python && \
    ln -sf /usr/bin/pip3 /usr/local/bin/pip

# Documentation toolchain (Sphinx + Breathe + RTD theme) available globally
RUN python3 -m pip install --no-cache-dir --break-system-packages \
    sphinx \
    breathe \
    sphinx-rtd-theme

# Install Node.js LTS (v20) from NodeSource
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs && \
    rm -rf /var/lib/apt/lists/*

# Install additional Go tools (versions pinned for Go 1.23 compatibility)
RUN go install github.com/go-delve/delve/cmd/dlv@v1.23.0 && \
    go install golang.org/x/tools/gopls@v0.17.0 && \
    go install github.com/golangci/golangci-lint/cmd/golangci-lint@v1.55.2 && \
    go install honnef.co/go/tools/cmd/staticcheck@2023.1.6

# Install pnpm
RUN npm install -g pnpm

# Install Taskfile
RUN curl --location https://taskfile.dev/install.sh | sh -s -- -b /usr/local/bin

# Install Changesets CLI
RUN npm install -g @changesets/cli

# Create dev user (minimal change, avoids root shell issues)
RUN useradd -ms /bin/zsh dev
# Allow dev user to sudo without password for package installs
RUN usermod -aG sudo dev && echo "dev ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/dev

# Prepare Codex config dir so a mounted volume keeps history with correct ownership
RUN mkdir -p /home/dev/.codex && \
    chown dev:dev /home/dev/.codex && \
    chmod 755 /home/dev/.codex

USER dev

# Set up PNPM storage
ENV PNPM_HOME="/home/dev/.pnpm"
ENV PATH="$PNPM_HOME:$PATH"

RUN mkdir -p $PNPM_HOME && \
    echo 'export PNPM_HOME=$PNPM_HOME' >> ~/.zshrc && \
    echo 'export PATH=$PNPM_HOME:$PATH' >> ~/.zshrc

# Install oh-my-zsh (non-interactive)
RUN sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" "" --unattended

# Configure oh-my-zsh: theme + git plugin
RUN sed -i 's/^ZSH_THEME=.*/ZSH_THEME="robbyrussell"/' ~/.zshrc && \
    sed -i 's/^plugins=.*/plugins=(git)/' ~/.zshrc

# Set environment variables
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Default shell
ENV SHELL=/bin/zsh
SHELL ["/bin/zsh", "-c"]

# Set working directory
WORKDIR /workspace
