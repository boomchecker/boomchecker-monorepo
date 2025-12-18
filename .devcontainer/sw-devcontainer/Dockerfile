# Start with the official Go image on Debian bookworm (has Python 3.11)
FROM golang:1.23-bookworm

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
    && rm -rf /var/lib/apt/lists/*

# Ensure `python` and `pip` commands are available
RUN ln -sf /usr/bin/python3 /usr/local/bin/python && \
    ln -sf /usr/bin/pip3 /usr/local/bin/pip

# Install Node.js LTS (v20) from NodeSource
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs && \
    rm -rf /var/lib/apt/lists/*

# Install additional Go tools
RUN go install github.com/go-delve/delve/cmd/dlv@latest && \
    go install golang.org/x/tools/gopls@v0.17.0 && \
    go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest && \
    go install honnef.co/go/tools/cmd/staticcheck@latest

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

# Set environment variables
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Default shell
SHELL ["/bin/bash", "-c"]

# Set working directory
WORKDIR /workspace
