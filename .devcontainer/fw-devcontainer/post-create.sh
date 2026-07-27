#!/usr/bin/env bash
# fw-devcontainer post-create: SSH/tool config + fetch large debug assets.
# Runs once after the container is created (see devcontainer.json).

# --- SSH keys + tool config (moved verbatim from the inline postCreateCommand) ---
mkdir -p ~boom/.ssh && cp -n ~boom/.ssh-host/* ~boom/.ssh/ 2>/dev/null
chown -R boom:boom ~boom/.ssh && chmod 700 ~boom/.ssh
find ~boom/.ssh -maxdepth 1 -type f ! -name '*.pub' -exec chmod 600 {} +
find ~boom/.ssh -maxdepth 1 -type f -name '*.pub' -exec chmod 644 {} +
chown -R boom:boom ~boom/.claude ~boom/.codex 2>/dev/null

# --- STM32H563 SVD for Cortex-Debug peripheral view (git-ignored, fetched here) ---
SVD=/workspace/fw/bom-stm32node/debug/STM32H563.svd
if [ ! -s "$SVD" ]; then
  echo "Fetching STM32H563 SVD ..."
  if curl -fsSL "https://raw.githubusercontent.com/modm-io/cmsis-svd-stm32/main/stm32h5/STM32H563.svd" -o "$SVD"; then
    echo "SVD downloaded to $SVD"
  else
    echo "WARN: SVD download failed - the debugger still works, just without the peripheral register view."
    rm -f "$SVD"
  fi
fi

true
