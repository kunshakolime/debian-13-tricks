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

log "Computing runtime dependencies (host dpkg-query)"
BIN="$CACHE/out/stage/Telegram"
DEPS=""
if [ -f "$BIN" ]; then
  PKGS="$(mktemp)"
  ldd "$BIN" | awk '/=> \//{print $3}' | sort -u | \
  while read -r path; do
    real="$(readlink -f "$path")"
    pkg=""
    for p in "$real" "$path"; do
      pkg="$(dpkg-query -S "$p" 2>/dev/null | head -1 | cut -d: -f1)" && break
    done
    [ -n "$pkg" ] && echo "$pkg" >> "$PKGS" || true
  done
  DEPS="$(sort -u "$PKGS" | grep -v '^$' | grep -v '^libpcre3$' | paste -sd, -)"
  rm -f "$PKGS"
fi

# libpcre3 is gone from trixie but the static-Qt toolchain linked PCRE1;
# bundle the legacy lib privately instead of depending on it.
PCRE_SRC=/lib/x86_64-linux-gnu/libpcre.so.3
if [ -e "$PCRE_SRC" ]; then
  mkdir -p "$CACHE/out/stage/pcre"
  cp -L "$PCRE_SRC" "$CACHE/out/stage/pcre/"
fi
echo "Depends: ${DEPS:-<none computed>}"

log "Building .deb inside container"
set +e
podman exec -e VERSION="$VERSION" -e DEPS="$DEPS" \
  "$CTR" bash /container/run.sh
rc=$?
set -e
if [ "$rc" -ne 0 ]; then
  # Client died early (e.g. Ctrl+C): podman does not forward signals,
  # so stop the in-container build explicitly.
  podman exec "$CTR" bash -c \
    'pkill -INT -f "/container/run.sh"; pkill -INT ninja; true' 2>/dev/null || true
fi

[ "$rc" -eq 0 ] || exit "$rc"

mkdir -p "$OUTDIR"
install -m 0644 "$CACHE/out/debs/ayugram_${VERSION}_amd64.deb" "$OUTDIR/"
echo
echo "Built: $OUTDIR/ayugram_${VERSION}_amd64.deb"
