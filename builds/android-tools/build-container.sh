#!/usr/bin/env bash
# build-container.sh — set up the build environment (image + container + source).
#
#   ./build-container.sh           # reuse cached image/container
#   ./build-container.sh --refresh # rebuild image, recreate container

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VERSION="$(cat "$SCRIPT_DIR/VERSION")"
IMAGE="android-tools-deb13:latest"
CTR="android-tools-deb13-builder"
CACHE="${ANDROID_TOOLS_BUILD_CACHE:-/var/tmp/android-tools-deb13-build}"

REFRESH=0
for arg in "$@"; do
  case "$arg" in
    --refresh) REFRESH=1 ;;
  esac
done

log() { printf '\n==> %s\n' "$*"; }

command -v podman >/dev/null || { echo "error: podman not found" >&2; exit 1; }

if [ "$REFRESH" -eq 1 ]; then
  log "Removing container (keeping image)"
  podman rm -f "$CTR" 2>/dev/null || true
fi

mkdir -p "$CACHE/source" "$CACHE/out"

if [ "$REFRESH" -eq 1 ] || ! podman image exists "$IMAGE"; then
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
    "$IMAGE" sleep infinity
fi

# Download source tarball if not cached.
TARBALL="$CACHE/source/android-tools-${VERSION}.tar.xz"
if [ ! -f "$TARBALL" ]; then
  log "Downloading android-tools v${VERSION} source"
  curl -fsSL -o "$TARBALL" \
    "https://github.com/nmeum/android-tools/releases/download/${VERSION}/android-tools-${VERSION}.tar.xz"
else
  log "Source tarball cached"
fi

log "Build environment ready"
