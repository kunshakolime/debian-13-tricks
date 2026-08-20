#!/usr/bin/env bash
# build-miracast-deb.sh — wrapper that calls build-container.sh + build-deb.sh.
#
#   ./build-miracast-deb.sh [OUTDIR]
#   ./build-miracast-deb.sh --clean       # full recompile
#   ./build-miracast-deb.sh --image       # rebuild Docker image, then build

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

EXTRA_ARGS=()
for arg in "$@"; do
  case "$arg" in
    --clean) EXTRA_ARGS+=("--clean") ;;
    --image)
      IMAGE="miracast-deb13:latest"
      podman build --progress=plain -t "$IMAGE" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR"
      ;;
    *) EXTRA_ARGS+=("$arg") ;;
  esac
done

"$SCRIPT_DIR/build-deb.sh" "${EXTRA_ARGS[@]}"
