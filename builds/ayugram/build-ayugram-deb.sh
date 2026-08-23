#!/usr/bin/env bash
# build-ayugram-deb.sh — one-stop build (calls build-container.sh + build-deb.sh).
#
#   ./build-ayugram-deb.sh [OUTDIR]    # incremental build (fast)
#   ./build-ayugram-deb.sh --reimage   # rebuild Docker image + recreate container
#   ./build-ayugram-deb.sh --recompile # wipe compiled artifacts only
#   ./build-ayugram-deb.sh --reimage --recompile  # both

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

DEB_ARGS=()
REIMAGE=0
OUTDIR=""

for arg in "$@"; do
  case "$arg" in
    --recompile) DEB_ARGS+=("--clean") ;;
    --reimage)   REIMAGE=1 ;;
    -*)          echo "unknown flag: $arg" >&2; exit 1 ;;
    *)           OUTDIR="$arg" ;;
  esac
done

[ -n "$OUTDIR" ] && DEB_ARGS+=("$OUTDIR")

if [ "$REIMAGE" -eq 1 ]; then
  "$SCRIPT_DIR/build-container.sh" --refresh
fi

"$SCRIPT_DIR/build-deb.sh" "${DEB_ARGS[@]}"
