#!/usr/bin/env bash
#
# build-miracast-deb.sh
#
# Builds the GTK4 Miracast sink (MiracleCast backend + GTK4 frontend) for
# Debian 13 (trixie) inside a persistent podman container, producing
# miracast-sink_<version>_amd64.deb in OUTDIR.
#
# The build environment is baked into the Dockerfile image; the container,
# the cloned MiracleCast source, and the meson build dir are all cached, so
# re-runs are incremental: only changed code is recompiled.
#
#   ./build-miracast-deb.sh [OUTDIR]      # OUTDIR defaults to $PWD
#   ./build-miracast-deb.sh --clean       # full recompile (keeps image + source)
#   ./build-miracast-deb.sh --image       # rebuild the Dockerfile image, then build
#
# Cache location (override with $MIRACAST_BUILD_CACHE):
#   /var/tmp/miracast-deb13-build
# To start from a fresh MiracleCast checkout / version bump:
#   rm -rf "$MIRACAST_BUILD_CACHE"/miraclecast "$MIRACAST_BUILD_CACHE"/out
#
# To re-bake the image when the Dockerfile changes: ./build-miracast-deb.sh --image
# (or just `podman build -t miracast-deb13:latest ./Dockerfile`).

set -euo pipefail

VERSION="0.1.0"
MIRACLE_COMMIT="0b7f1f1f6586dc65ff480f3cda5c2170a70aa020"   # upstream master, 2026-03-09
IMAGE="miracast-deb13:latest"
CTR="miracast-deb13-builder"
CACHE="${MIRACAST_BUILD_CACHE:-/var/tmp/miracast-deb13-build}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

CLEAN=0
REBUILD_IMAGE=0
OUTDIR="$PWD"
for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=1 ;;
    --image) REBUILD_IMAGE=1 ;;
    *) OUTDIR="$arg" ;;
  esac
done

log() { printf '\n==> %s\n' "$*"; }

command -v podman >/dev/null || { echo "error: podman not found" >&2; exit 1; }

mkdir -p "$CACHE/miraclecast" "$CACHE/out" "$OUTDIR"

if [ "$REBUILD_IMAGE" -eq 1 ] || ! podman image exists "$IMAGE"; then
  log "Building image '$IMAGE' (Dockerfile)"
  podman build -t "$IMAGE" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR"
else
  log "Image '$IMAGE' cached"
fi

if podman container exists "$CTR"; then
  log "Reusing build container '$CTR'"
  podman start "$CTR" >/dev/null 2>&1 || true
else
  log "Creating persistent build container '$CTR' (one-time)"
  podman run -d --name "$CTR" --network=host \
    -v "$CACHE":/build:Z \
    -v "$SCRIPT_DIR/container":/container:Z \
    -v "$SCRIPT_DIR/src":/appsrc:Z \
    "$IMAGE" sleep infinity
fi

if [ ! -d "$CACHE/miraclecast/.git" ]; then
  log "Cloning MiracleCast (one-time, pinned to $MIRACLE_COMMIT)"
  git clone https://github.com/albfan/miraclecast.git "$CACHE/miraclecast"
  git -C "$CACHE/miraclecast" checkout -q "$MIRACLE_COMMIT"
else
  log "MiracleCast source cached (re-checking pinned commit)"
  git -C "$CACHE/miraclecast" checkout -q "$MIRACLE_COMMIT"
fi

if [ "$CLEAN" -eq 1 ]; then
  log "Wiping cached build dir (full recompile)"
  podman exec "$CTR" bash -c 'rm -rf /build/out/build /build/out/stage /build/out/debpkg /build/out/debian'
fi

log "Running in-container build"
podman exec -e VERSION="$VERSION" \
  "$CTR" bash /container/run.sh

install -m 0644 "$CACHE/out/debs/miracast-sink_${VERSION}_amd64.deb" "$OUTDIR/"
echo
echo "Built: $OUTDIR/miracast-sink_${VERSION}_amd64.deb"
echo "Install on Debian 13:  sudo apt install ./miracast-sink_${VERSION}_amd64.deb"
echo "Cache kept at:         $CACHE (reuse for fast re-runs)"