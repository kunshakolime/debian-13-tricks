#!/usr/bin/env bash
#
# build-scrcpy-deb.sh
#
# Packages the official scrcpy static release into a Debian .deb using the
# standard FHS layout (/usr/bin, /usr/share/man, /usr/share/doc).
#
# The upstream static tarball bundles its own adb binary. This build strips
# it and instead Depends on Debian's `adb` package; scrcpy then resolves
# /usr/bin/adb from PATH.
#
#   ./build-scrcpy-deb.sh [OUTDIR]      # OUTDIR defaults to $PWD
#
# Bump VERSION below to track the latest release at
# https://github.com/Genymobile/scrcpy/releases

set -euo pipefail

VERSION="4.1"
TARBALL_URL="https://github.com/Genymobile/scrcpy/releases/download/v${VERSION}/scrcpy-linux-x86_64-v${VERSION}.tar.gz"

OUTDIR="$PWD"
[ $# -ge 1 ] && OUTDIR="$1"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

log() { printf '\n==> %s\n' "$*"; }

command -v curl >/dev/null || { echo "error: curl not found" >&2; exit 1; }
command -v dpkg-deb >/dev/null || { echo "error: dpkg-deb not found" >&2; exit 1; }
command -v dpkg-shlibdeps >/dev/null || { echo "error: dpkg-shlibdeps not found (install dpkg-dev)" >&2; exit 1; }

mkdir -p "$OUTDIR"

log "Downloading official scrcpy v${VERSION} static release"
curl -fsSL -o "$WORK/scrcpy.tar.gz" "$TARBALL_URL"
tar -xzf "$WORK/scrcpy.tar.gz" -C "$WORK"

SRC="$WORK/scrcpy-linux-x86_64-v${VERSION}"
PACKAGE="$WORK/debpkg"
mkdir -p "$PACKAGE/DEBIAN" "$PACKAGE/usr/bin" "$PACKAGE/usr/share/doc/scrcpy" \
         "$PACKAGE/usr/share/man/man1" "$PACKAGE/usr/share/scrcpy"

log "Staging files (standard FHS layout, bundled adb stripped)"
install -m 0755 "$SRC/scrcpy" "$PACKAGE/usr/bin/scrcpy"
install -m 0644 "$SRC/scrcpy-server" "$PACKAGE/usr/bin/scrcpy-server"
install -m 0644 "$SRC/scrcpy.png" "$SRC/disconnected.png" "$PACKAGE/usr/share/scrcpy/"
gzip -c "$SRC/scrcpy.1" > "$PACKAGE/usr/share/man/man1/scrcpy.1.gz"
cp "$SRC/LICENSE" "$PACKAGE/usr/share/doc/scrcpy/copyright"

log "Computing shared-library dependencies"
mkdir -p "$WORK/debian"
cat > "$WORK/debian/control" <<'CTL'
Source: scrcpy
Section: net
Priority: optional
Maintainer: Debian 13 build <root@localhost>

Package: scrcpy
Architecture: amd64
Description: Display and control your Android device (screen mirroring)
CTL
DEPS="$(cd "$WORK" && dpkg-shlibdeps -O "$PACKAGE/usr/bin/scrcpy" 2>/dev/null)"
DEPS="${DEPS#shlibs:Depends=}"
DEPS="${DEPS:+$DEPS, }adb"
echo "Depends: $DEPS"

cat > "$PACKAGE/DEBIAN/control" <<EOF
Package: scrcpy
Version: ${VERSION}
Section: net
Priority: optional
Architecture: amd64
Maintainer: Debian 13 build <root@localhost>
Depends: ${DEPS}
Homepage: https://github.com/Genymobile/scrcpy
Description: Display and control your Android device (screen mirroring)
 A lightweight display and control of Android devices (screen mirroring)
 over USB or Wi-Fi.
 .
 The adb bundled in the official static release is stripped; this package
 uses Debian's adb package instead.
EOF

log "Building scrcpy_${VERSION}_amd64.deb"
dpkg-deb --build --root-owner-group "$PACKAGE" "$WORK/scrcpy_${VERSION}_amd64.deb"
install -m 0644 "$WORK/scrcpy_${VERSION}_amd64.deb" "$OUTDIR/"
echo
echo "Built: $OUTDIR/scrcpy_${VERSION}_amd64.deb"
echo "Install on Debian 13:  sudo apt install ./scrcpy_${VERSION}_amd64.deb"
