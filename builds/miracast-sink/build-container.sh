#!/usr/bin/env bash
# build-container.sh — set up the build environment (image + container + source).
#
# Run this first if you want to pre-pull / pre-build before the actual compile.
# Subsequent runs are instant (everything cached).

set -euo pipefail

MIRACLE_COMMIT="0b7f1f1f6586dc65ff480f3cda5c2170a70aa020"
IMAGE="miracast-deb13:latest"
CTR="miracast-deb13-builder"
CACHE="${MIRACAST_BUILD_CACHE:-/var/tmp/miracast-deb13-build}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

log() { printf '\n==> %s\n' "$*"; }

command -v podman >/dev/null || { echo "error: podman not found" >&2; exit 1; }

mkdir -p "$CACHE/miraclecast" "$CACHE/out"

# 1. image
if ! podman image exists "$IMAGE"; then
  log "Building image '$IMAGE' (Dockerfile)"
  podman build --progress=plain -t "$IMAGE" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR"
else
  log "Image '$IMAGE' cached"
fi

# 2. container
if podman container exists "$CTR"; then
  log "Reusing container '$CTR'"
  podman start "$CTR" >/dev/null 2>&1 || true
else
  log "Creating persistent container '$CTR'"
  podman run -d --name "$CTR" --network=host \
    -v "$CACHE":/build:Z \
    -v "$SCRIPT_DIR/container":/container:Z \
    -v "$SCRIPT_DIR/src":/appsrc:Z \
    "$IMAGE" sleep infinity
fi

# 3. miraclecast source
if [ ! -d "$CACHE/miraclecast/.git" ]; then
  log "Cloning MiracleCast (pinned to $MIRACLE_COMMIT)"
  git clone https://github.com/albfan/miraclecast.git "$CACHE/miraclecast"
  git -C "$CACHE/miraclecast" checkout -q "$MIRACLE_COMMIT"
else
  log "MiracleCast source cached"
  git -C "$CACHE/miraclecast" checkout -q "$MIRACLE_COMMIT"
fi

log "Build environment ready"
