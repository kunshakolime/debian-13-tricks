#!/usr/bin/env bash
# run.sh — in-container build for AyuGramDesktop.
#
# Builds the Telegram binary using the upstream build system,
# then packages it as a .deb for Debian 13 (trixie).
#
# Output: /build/out/debs/ayugram_<ver>_amd64.deb

set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

log() { printf '\n==> %s\n' "$*"; }

VERSION="${VERSION:?VERSION must be set}"
SRC_DIR="/build/source/AyuGramDesktop"

export CCACHE_DIR=/build/ccache
mkdir -p "$CCACHE_DIR"

mkdir -p /build/out/build
exec 9>/build/out/build.lock
flock -n 9 || { echo "ERROR: another build is already running" >&2; exit 1; }

# Forward TERM/INT to the compiler so nothing is left running behind us.
on_signal() {
  trap - INT TERM
  log "Interrupted — stopping compiler"
  pkill -TERM -P $$ 2>/dev/null || true
  sleep 2
  pkill -KILL -P $$ 2>/dev/null || true
  exit 130
}
trap on_signal INT TERM

# --------------------------------------------------------------- build
if [ -f "$SRC_DIR/out/CMakeCache.txt" ] \
   && ! grep -q "COMPILER_LAUNCHER" "$SRC_DIR/out/CMakeCache.txt"; then
  log "Reconfiguring build dir (enable ccache)"
  rm -f /build/out/build/.configured
fi

if [ ! -f /build/out/build/.configured ]; then
  log "Configuring AyuGramDesktop"
  cd "$SRC_DIR/Telegram"
  ./configure.sh \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_PREFIX_PATH=/usr/local \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DTDESKTOP_API_ID=2040 \
    -DTDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627
  touch /build/out/build/.configured
else
  log "Build dir cached (incremental)"
fi

log "Compiling"
cd "$SRC_DIR"
cmake --build out --config Release -j$(nproc) 2>&1

# --------------------------------------------------------- staged install
cd "$SRC_DIR"
log "Staging install"
rm -rf /build/out/stage
mkdir -p /build/out/stage

BIN=""
for f in out/Release/AyuGram out/Release/Telegram out/AyuGram; do
  if [ -f "$f" ]; then BIN="$f"; break; fi
done
if [ -z "$BIN" ]; then
  echo "ERROR: compiled binary not found under $SRC_DIR/out" >&2
  exit 1
fi
log "Staging binary: $BIN"
cp -a "$BIN" /build/out/stage/Telegram

# Also grab any .desktop files, icons, etc.
find "$SRC_DIR" -name "*.desktop" -exec cp {} /build/out/stage/ \; 2>/dev/null || true

# ------------------------------------------------- dpkg assembly
log "Assembling .deb"
DEBPKG="/build/out/debpkg"
rm -rf "$DEBPKG"
mkdir -p "$DEBPKG/DEBIAN"
mkdir -p "$DEBPKG/usr/bin"
mkdir -p "$DEBPKG/usr/share/applications"
mkdir -p "$DEBPKG/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$DEBPKG/usr/share/doc/ayugram"

# Install binary (private dir + wrapper for bundled legacy libs)
APPLIB="$DEBPKG/usr/lib/ayugram"
mkdir -p "$APPLIB"
install -s -m 0755 /build/out/stage/Telegram "$APPLIB/AyuGram.bin"
if [ -d /build/out/stage/pcre ]; then
  cp -a /build/out/stage/pcre/. "$APPLIB"/
fi
cat > "$DEBPKG/usr/bin/ayugram" << 'EOF'
#!/bin/sh
export LD_LIBRARY_PATH="/usr/lib/ayugram${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec /usr/lib/ayugram/AyuGram.bin "$@"
EOF
chmod 0755 "$DEBPKG/usr/bin/ayugram"

# Install desktop file
DESKTOP_FILE=$(find /build/out/stage -name "*.desktop" -print -quit 2>/dev/null || true)
if [ -n "$DESKTOP_FILE" ] && [ -f "$DESKTOP_FILE" ]; then
  sed -i 's|^Exec=.*|Exec=ayugram|' "$DESKTOP_FILE"
  sed -i 's|^Name=.*|Name=AyuGram|' "$DESKTOP_FILE"
  install -m 0644 "$DESKTOP_FILE" "$DEBPKG/usr/share/applications/ayugram.desktop"
fi

# Compute shared-library dependencies
log "Computing shared-library dependencies"
if [ -z "${DEPS:-}" ]; then
  DEPS="$(cd /build/out && dpkg-shlibdeps -O \
    debpkg/usr/lib/ayugram/AyuGram.bin \
    2>/dev/null || true)"
  DEPS="${DEPS#shlibs:Depends=}"
  DEPS="${DEPS// /}"
fi
echo "Computed Depends: $DEPS"

cat > "$DEBPKG/DEBIAN/control" << EOF
Package: ayugram
Version: ${VERSION}
Section: net
Priority: optional
Architecture: amd64
Maintainer: Debian 13 build <root@localhost>
Depends: ${DEPS}
Homepage: https://github.com/AyuGram/AyuGramDesktop
Description: AyuGram - Telegram Desktop fork with extra features
 AyuGram is a fork of Telegram Desktop with additional features
 including anti-recall, message translation, and more.
 .
 Built from AyuGramDesktop v${VERSION}.
EOF

cat > "$DEBPKG/usr/share/doc/ayugram/changelog.Debian" << CL
ayugram (${VERSION}) trixie; urgency=medium

  * Initial build: AyuGramDesktop v${VERSION}.

 -- Debian 13 build <root@localhost>  $(date -R)
CL
gzip -9n "$DEBPKG/usr/share/doc/ayugram/changelog.Debian"

log "Building .deb"
mkdir -p /build/out/debs
cd /build/out
dpkg-deb --build --root-owner-group debpkg "ayugram_${VERSION}_amd64.deb"
cp -f "ayugram_${VERSION}_amd64.deb" /build/out/debs/

log "Verifying"
dpkg-deb --info /build/out/debs/"ayugram_${VERSION}_amd64.deb" >/dev/null
echo "DONE: /build/out/debs/ayugram_${VERSION}_amd64.deb"
