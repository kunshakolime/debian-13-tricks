#!/usr/bin/env bash
# build-deb.sh — compile and package miracast-sink inside the container.
#
# Ensures the build environment exists first (calls build-container.sh if
# needed), then runs the in-container build. Output: miracast-sink_<ver>_amd64.deb
#
#   ./build-deb.sh [OUTDIR]    # defaults to $PWD
#   ./build-deb.sh --clean     # wipe build artifacts, full recompile

set -euo pipefail

VERSION="0.2.0"
IMAGE="miracast-deb13:latest"
CTR="miracast-deb13-builder"
CACHE="${MIRACAST_BUILD_CACHE:-/var/tmp/miracast-deb13-build}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

CLEAN=0
OUTDIR="$PWD"
for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=1 ;;
    *) OUTDIR="$arg" ;;
  esac
done

log() { printf '\n==> %s\n' "$*"; }

# make sure the build environment is ready
"$SCRIPT_DIR/build-container.sh"

# optional: full recompile
if [ "$CLEAN" -eq 1 ]; then
  log "Wiping build artifacts (full recompile)"
  podman exec "$CTR" bash -c 'rm -rf /build/out/build-miracle /build/out/build-app /build/out/stage /build/out/debpkg /build/out/debian'
fi

log "Building .deb inside container"
podman exec -e VERSION="$VERSION" \
  "$CTR" bash /container/run.sh

mkdir -p "$OUTDIR"
install -m 0644 "$CACHE/out/debs/miracast-sink_${VERSION}_amd64.deb" "$OUTDIR/"
echo
echo "Built: $OUTDIR/miracast-sink_${VERSION}_amd64.deb"
