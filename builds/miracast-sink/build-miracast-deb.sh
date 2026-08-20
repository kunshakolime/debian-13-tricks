#!/usr/bin/env bash
# build-miracast-deb.sh — one-stop build (calls build-container.sh + build-deb.sh).
#
#   ./build-miracast-deb.sh [OUTDIR]    # incremental build (fast)
#   ./build-miracast-deb.sh --rebuild   # nuke everything: container, image, and compiled artifacts
#   ./build-miracast-deb.sh --recompile # wipe only compiled artifacts (keep container + image)
#   ./build-miracast-deb.sh --reimage   # rebuild just the Docker image (keep compiled artifacts)

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
  IMAGE="miracast-deb13:latest"
  podman build --progress=plain -t "$IMAGE" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR"
fi

"$SCRIPT_DIR/build-deb.sh" "${DEB_ARGS[@]}"
