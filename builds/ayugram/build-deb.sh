#!/usr/bin/env bash
# build-deb.sh — compile and package AyuGramDesktop inside the container.
#
# Ensures the build environment exists first (calls build-container.sh if
# needed), then runs the in-container build. Output: ayugram_<ver>_amd64.deb
#
#   ./build-deb.sh [OUTDIR]    # defaults to $PWD
#   ./build-deb.sh --clean     # wipe build artifacts, full recompile

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VERSION="$(cat "$SCRIPT_DIR/VERSION")"
CTR="ayugram-deb13-builder"
CACHE="${AYUGRAM_BUILD_CACHE:-/var/tmp/ayugram-deb13-build}"

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
  podman exec "$CTR" bash -c \
    'rm -rf /build/out/build /build/out/stage /build/out/debpkg /build/source/AyuGramDesktop/out'
fi

log "Building .deb inside container"
set +e
podman exec -e VERSION="$VERSION" \
  "$CTR" bash /container/run.sh
rc=$?
set -e
if [ "$rc" -ne 0 ]; then
  # Client died early (e.g. Ctrl+C): podman does not forward signals,
  # so stop the in-container build explicitly.
  podman exec "$CTR" bash -c \
    'pkill -INT -f "/container/run.sh"; pkill -INT ninja; true' 2>/dev/null || true
fi
exit "$rc"

mkdir -p "$OUTDIR"
install -m 0644 "$CACHE/out/debs/ayugram_${VERSION}_amd64.deb" "$OUTDIR/"
echo
echo "Built: $OUTDIR/ayugram_${VERSION}_amd64.deb"
