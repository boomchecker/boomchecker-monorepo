#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/python/venv"

if ! python3 -m venv "$VENV_DIR"; then
    echo "Creating venv failed. Install python3-venv and re-run." >&2
    exit 1
fi

"$VENV_DIR/bin/python" -m pip install --upgrade pip
"$VENV_DIR/bin/python" -m pip install -r "$SCRIPT_DIR/requirements.txt"

echo "lms-filter setup complete: $VENV_DIR"

