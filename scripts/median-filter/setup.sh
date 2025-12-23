#!/usr/bin/env bash
set -euo pipefail

echo "==> Setting up median-filter environment"

# Determine sudo prefix
if [[ $(id -u) -ne 0 ]] && command -v sudo >/dev/null 2>&1; then
    SUDO_PREFIX="sudo"
else
    SUDO_PREFIX=""
fi

$SUDO_PREFIX apt-get update
$SUDO_PREFIX apt-get install python3-venv -y

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_DIR="$SCRIPT_DIR/python"
REQUIREMENTS_FILE="$PYTHON_DIR/requirements.txt"

echo "==> Installing Python dependencies (system Python with PIP_BREAK_SYSTEM_PACKAGES=1)"
VENV_DIR="$PYTHON_DIR/venv"  
python3 -m venv "$VENV_DIR"  
source "$VENV_DIR/bin/activate"  
pip install --upgrade pip  
pip install -r "$REQUIREMENTS_FILE"  

echo "==> Ensuring ffmpeg is installed"
if command -v ffmpeg >/dev/null 2>&1; then
    echo "ffmpeg already present ($(command -v ffmpeg))"
else
    if command -v apt-get >/dev/null 2>&1; then
        if [[ $(id -u) -ne 0 ]] && command -v sudo >/dev/null 2>&1; then
            SUDO_PREFIX="sudo"
        else
            SUDO_PREFIX=""
        fi
        $SUDO_PREFIX apt-get update
        $SUDO_PREFIX apt-get install -y ffmpeg
    else
        echo "Please install ffmpeg manually for your platform." >&2
        exit 1
    fi
fi

echo "==> median-filter setup complete."
