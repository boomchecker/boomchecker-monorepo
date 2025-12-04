#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REQUIREMENTS_FILE="$SCRIPT_DIR/python/requirements.txt"

echo "==> Installing Python dependencies (pip install -r requirements.txt)"
python3 -m pip install --upgrade pip
python3 -m pip install -r "$REQUIREMENTS_FILE"

echo "==> Ensuring ffmpeg is installed"
if command -v ffmpeg >/dev/null 2>&1; then
    echo "ffmpeg already present ($(command -v ffmpeg))"
else
    if command -v apt-get >/dev/null 2>&1; then
        apt-get update
        apt-get install -y ffmpeg
    else
        echo "Please install ffmpeg manually for your platform." >&2
        exit 1
    fi
fi

echo "==> median-filter setup complete."
