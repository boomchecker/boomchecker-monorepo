# Start with the official Go image
FROM golang:1.23-bullseye

# Install basic development tools
RUN apt-get update && apt-get install -y \
    git \
    curl \
    wget \
    make \
    build-essential \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install additional Go tools
RUN go install github.com/go-delve/delve/cmd/dlv@latest && \
    go install golang.org/x/tools/gopls@v0.17.0 && \
    go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest && \
    go install honnef.co/go/tools/cmd/staticcheck@latest

# Install Node.js (LTS) and pnpm (best-effort to avoid failing offline builds)
RUN apt-get update && apt-get install -y nodejs npm || { echo "Warning: apt install nodejs/npm failed (likely no network). Install inside container when available."; true; } && \
    npm install -g pnpm || { echo "Warning: npm install pnpm failed (likely no network). Install inside container when available."; true; }

# Install Taskfile and Changesets CLI (best-effort)
RUN curl -sL https://taskfile.dev/install.sh | sh -s -- -b /usr/local/bin || { echo "Warning: Taskfile install failed (likely no network). Install inside container when available."; true; }
RUN npm install -g @changesets/cli || { echo "Warning: npm install @changesets/cli failed (likely no network). Install inside container when available."; true; }
    
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
