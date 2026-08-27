#!/usr/bin/env bash
# build-container.sh — set up the build environment (image + container + source).
#
#   ./build-container.sh           # reuse cached image/container
#   ./build-container.sh --refresh # rebuild image, recreate container

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VERSION="$(cat "$SCRIPT_DIR/VERSION")"
IMAGE="ayugram-deb13:latest"
CTR="ayugram-deb13-builder"
CACHE="${AYUGRAM_BUILD_CACHE:-/var/tmp/ayugram-deb13-build}"

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
  podman run -d --name "$CTR" --network=host --user root \
    -v "$CACHE":/build:Z \
    -v "$SCRIPT_DIR/build-inside.sh":/build-inside.sh:Z \
    "$IMAGE" sleep infinity
fi

# Clone source if not cached.
SRC_DIR="$CACHE/source/AyuGramDesktop"
if [ ! -d "$SRC_DIR" ]; then
  log "Cloning AyuGramDesktop v${VERSION} (shallow)"
  git clone --depth=1 --branch "v${VERSION}" \
    https://github.com/AyuGram/AyuGramDesktop.git "$SRC_DIR" 2>/dev/null || \
  git clone --depth=1 \
    https://github.com/AyuGram/AyuGramDesktop.git "$SRC_DIR"
  cd "$SRC_DIR" && git submodule update --init --recursive && cd -
else
  log "Source cached"
fi

log "Build environment ready"
