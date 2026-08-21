#!/usr/bin/env bash
# build-container.sh — set up the build environment (image + container + source).
#
#   ./build-container.sh           # reuse cached image/container
#   ./build-container.sh --fresh   # remove container + image, rebuild everything
#   ./build-container.sh --refresh # rebuild image, recreate container (keep source cache)

set -euo pipefail

MIRACLE_COMMIT="0b7f1f1f6586dc65ff480f3cda5c2170a70aa020"
IMAGE="miracast-deb13:latest"
CTR="miracast-deb13-builder"
CACHE="${MIRACAST_BUILD_CACHE:-/var/tmp/miracast-deb13-build}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

FRESH=0
REFRESH=0
for arg in "$@"; do
  case "$arg" in
    --fresh)  FRESH=1 ;;
    --refresh) REFRESH=1 ;;
  esac
done

log() { printf '\n==> %s\n' "$*"; }

command -v podman >/dev/null || { echo "error: podman not found" >&2; exit 1; }

if [ "$FRESH" -eq 1 ]; then
  log "Removing old container and image"
  podman rm -f "$CTR" 2>/dev/null || true
  podman rmi "$IMAGE" 2>/dev/null || true
elif [ "$REFRESH" -eq 1 ]; then
  log "Removing container (keeping image)"
  podman rm -f "$CTR" 2>/dev/null || true
fi

mkdir -p "$CACHE/miraclecast" "$CACHE/out"

if [ "$FRESH" -eq 1 ] || [ "$REFRESH" -eq 1 ] || ! podman image exists "$IMAGE"; then
  log "Building image '$IMAGE' (Dockerfile)"
  podman build --progress=plain -t "$IMAGE" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR"
else
  log "Image '$IMAGE' cached"
fi

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

if [ ! -d "$CACHE/miraclecast/.git" ]; then
  log "Cloning MiracleCast (pinned to $MIRACLE_COMMIT)"
  git clone https://github.com/albfan/miraclecast.git "$CACHE/miraclecast"
  git -C "$CACHE/miraclecast" checkout -q "$MIRACLE_COMMIT"
else
  log "MiracleCast source cached"
  git -C "$CACHE/miraclecast" checkout -q "$MIRACLE_COMMIT"
fi

log "Build environment ready"
