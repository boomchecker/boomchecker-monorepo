#!/usr/bin/env bash
# Container entrypoint for the FW devcontainer. Runs as root (containerUser: root)
# on every container start, BEFORE the interactive `boom` shells attach.
#
# Reclaim ownership of bind-mounted /workspace files that ended up owned by root
# — e.g. build artifacts from an earlier image where builds ran as root, or
# generated dirs (fw/*/build, managed_components, boomdetect). Without this,
# `boom` (UID 1000) can't delete or rebuild them: a root-owned dir is group r-x
# with no write bit, so `rm -rf build` / `task clean` fail with EPERM and a
# stale CMake cache keeps pointing the build at the old toolchain path.
#
# The `! -uid 1000` filter chowns only what isn't already boom-owned, so after
# the first pass every later start is a cheap no-op.
find /workspace -xdev ! -uid 1000 -exec chown 1000:1000 {} + 2>/dev/null || true

exec sleep infinity
