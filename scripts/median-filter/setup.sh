#!/usr/bin/env bash
set -euo pipefail

echo "==> Setting up median-filter environment"

# Determine apt + sudo availability to avoid noisy failures in restricted devcontainers
APT_CAN_INSTALL=false
APT_PREFIX=""
if command -v apt-get >/dev/null 2>&1; then
    if [[ $(id -u) -eq 0 ]]; then
        APT_CAN_INSTALL=true
    elif command -v sudo >/dev/null 2>&1; then
        APT_CAN_INSTALL=true
        APT_PREFIX="sudo"
    fi
fi

# Install python3-venv only when apt-get is usable (root/sudo). Otherwise, expect it to be preinstalled.
if [[ "$APT_CAN_INSTALL" == true ]]; then
    $APT_PREFIX apt-get update
    $APT_PREFIX apt-get install -y python3-venv
else
    echo "Skipping apt-get install for python3-venv (no root/sudo available). Ensure python3-venv is installed." >&2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_DIR="$SCRIPT_DIR/python"
REQUIREMENTS_FILE="$PYTHON_DIR/requirements.txt"

echo "==> Installing Python dependencies (system Python with PIP_BREAK_SYSTEM_PACKAGES=1)"
VENV_DIR="$PYTHON_DIR/venv"
if ! python3 -m venv "$VENV_DIR"; then
    echo "Creating venv failed. Install python3-venv (e.g., sudo apt-get install -y python3-venv) and re-run." >&2
    exit 1
fi
source "$VENV_DIR/bin/activate"
pip install --upgrade pip
pip install -r "$REQUIREMENTS_FILE"

echo "==> Ensuring ffmpeg is installed"
if command -v ffmpeg >/dev/null 2>&1; then
    echo "ffmpeg already present ($(command -v ffmpeg))"
else
    if [[ "$APT_CAN_INSTALL" == true ]]; then
        $APT_PREFIX apt-get update
        $APT_PREFIX apt-get install -y ffmpeg
    else
        echo "ffmpeg missing and apt-get not usable (no root/sudo). Install ffmpeg manually and re-run." >&2
        exit 1
    fi
fi

echo "==> median-filter setup complete."
