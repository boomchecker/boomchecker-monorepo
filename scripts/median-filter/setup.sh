#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_DIR="$SCRIPT_DIR/python"
REQUIREMENTS_FILE="$PYTHON_DIR/requirements.txt"

echo "==> Installing Python dependencies (system Python with PIP_BREAK_SYSTEM_PACKAGES=1)"
export PIP_BREAK_SYSTEM_PACKAGES=1
python3 -m pip install --upgrade pip
python3 -m pip install -r "$REQUIREMENTS_FILE"

echo "==> Ensuring ffmpeg is installed"
if command -v ffmpeg >/dev/null 2>&1; then
    echo "ffmpeg already present ($(command -v ffmpeg))"
else
    if command -v apt-get >/dev/null 2>&1; then
        if [[ $(id -u) -ne 0 ]] && command -v sudo >/dev/null 2>&1; then
            SUDO="sudo"
        else
            SUDO=""
        fi
        $SUDO apt-get update
        $SUDO apt-get install -y ffmpeg
    else
        echo "Please install ffmpeg manually for your platform." >&2
        exit 1
    fi
fi

echo "==> median-filter setup complete."
