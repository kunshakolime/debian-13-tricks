#!/usr/bin/env bash
# build-miracast-deb.sh — one-stop build (calls build-container.sh + build-deb.sh).
#
#   ./build-miracast-deb.sh [OUTDIR]    # incremental build (fast)
#   ./build-miracast-deb.sh --rebuild   # nuke everything: container, image, and compiled artifacts
#   ./build-miracast-deb.sh --recompile # wipe only compiled artifacts (keep container + image)
#   ./build-miracast-deb.sh --reimage   # rebuild image + recreate container (keep source cache)

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

DEB_ARGS=()
REBUILD=0
REIMAGE=0
OUTDIR=""

for arg in "$@"; do
  case "$arg" in
    --rebuild)   REBUILD=1 ;;
    --recompile) DEB_ARGS+=("--clean") ;;
    --reimage)   REIMAGE=1 ;;
    -*)          echo "unknown flag: $arg" >&2; exit 1 ;;
    *)           OUTDIR="$arg" ;;
  esac
done

[ -n "$OUTDIR" ] && DEB_ARGS+=("$OUTDIR")

if [ "$REBUILD" -eq 1 ]; then
  "$SCRIPT_DIR/build-container.sh" --fresh
  DEB_ARGS+=("--clean")
elif [ "$REIMAGE" -eq 1 ]; then
  "$SCRIPT_DIR/build-container.sh" --refresh
  DEB_ARGS+=("--clean")
fi

"$SCRIPT_DIR/build-deb.sh" "${DEB_ARGS[@]}"
