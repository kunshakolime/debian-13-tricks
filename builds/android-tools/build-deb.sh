#!/usr/bin/env bash
# build-deb.sh — compile and package android-tools inside the container.
#
# Ensures the build environment exists first (calls build-container.sh if
# needed), then runs the in-container build. Output: android-tools_<ver>_amd64.deb
#
#   ./build-deb.sh [OUTDIR]    # defaults to $PWD
#   ./build-deb.sh --clean     # wipe build artifacts, full recompile

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VERSION="$(cat "$SCRIPT_DIR/VERSION")"
IMAGE="android-tools-deb13:latest"
CTR="android-tools-deb13-builder"
CACHE="${ANDROID_TOOLS_BUILD_CACHE:-/var/tmp/android-tools-deb13-build}"

CLEAN=0
OUTDIR="$PWD"
for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=1 ;;
    *) OUTDIR="$arg" ;;
  esac
done

log() { printf '\n==> %s\n' "$*"; }

# Make sure the build environment is ready.
"$SCRIPT_DIR/build-container.sh"

# Optional: full recompile.
if [ "$CLEAN" -eq 1 ]; then
  log "Wiping build artifacts (full recompile)"
  podman exec "$CTR" bash -c 'rm -rf /build/out/build /build/out/stage /build/out/debpkg /build/out/debian'
fi

log "Building .deb inside container"
podman exec -e VERSION="$VERSION" \
  "$CTR" bash /build-inside.sh

mkdir -p "$OUTDIR"
install -m 0644 "$CACHE/out/debs/android-tools_${VERSION}_amd64.deb" "$OUTDIR/"
echo
echo "Built: $OUTDIR/android-tools_${VERSION}_amd64.deb"
