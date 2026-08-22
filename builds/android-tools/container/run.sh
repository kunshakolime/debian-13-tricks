#!/usr/bin/env bash
# run.sh — in-container build for android-tools (nmeum).
#
# Builds all tools from source, then packages only the extras —
# adb, fastboot, and simg2img are left to Debian's packages.
#
# Output: /build/out/debs/android-tools_<ver>_amd64.deb

set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

log() { printf '\n==> %s\n' "$*"; }

VERSION="${VERSION:?VERSION must be set}"
SRC_TARBALL="/build/source/android-tools-${VERSION}.tar.xz"

# --------------------------------------------------------------- build
if [ ! -d /build/out/build ]; then
  log "Extracting source"
  mkdir -p /build/out/build
  tar -xf "$SRC_TARBALL" -C /build/out/build --strip-components=1 2>/dev/null || true

  log "cmake: configure (Release, Ninja)"
  cmake -S /build/out/build -B /build/out/build/_build \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja \
    -DANDROID_TOOLS_USE_BUNDLED_FMT=OFF \
    -DANDROID_TOOLS_USE_BUNDLED_LIBUSB=OFF
else
  log "Build dir cached (incremental)"
fi

log "cmake: compile"
cmake --build /build/out/build/_build

# --------------------------------------------------------- staged install
log "Staging install (full)"
rm -rf /build/out/stage
DESTDIR=/build/out/stage cmake --install /build/out/build/_build

# ------------------------------------------------- selective removal
log "Removing tools provided by Debian from staged install"
rm -f /build/out/stage/usr/bin/adb
rm -f /build/out/stage/usr/bin/fastboot
rm -f /build/out/stage/usr/bin/simg2img
rm -f /build/out/stage/usr/bin/img2simg
rm -f /build/out/stage/usr/bin/append2simg
rm -f /build/out/stage/usr/share/bash-completion/completions/adb
rm -f /build/out/stage/usr/share/bash-completion/completions/fastboot
rm -f /build/out/stage/usr/share/android-tools/completions/adb
rm -f /build/out/stage/usr/share/android-tools/completions/fastboot
rm -f /build/out/stage/usr/share/zsh/site-functions/_adb
rm -f /build/out/stage/usr/share/zsh/site-functions/_fastboot
rm -f /build/out/stage/usr/share/man/man1/adb.1.gz

# -------------------------------------------------- dpkg assembly
log "Assembling package metadata"
mkdir -p /build/out/debpkg/DEBIAN
mkdir -p /build/out/debpkg/usr/share/doc/android-tools
mkdir -p /build/out/debian

cp -a /build/out/stage/usr/* /build/out/debpkg/usr/

# Stub control for dpkg-shlibdeps.
cat > /build/out/debian/control << 'CTL'
Source: android-tools
Section: utils
Priority: optional
Maintainer: Debian 13 build <root@localhost>

Package: android-tools
Architecture: any
Description: Android platform tools (extra utilities)
CTL

log "Computing shared-library dependencies"
DEPS="$(cd /build/out && dpkg-shlibdeps -O \
  debpkg/usr/bin/lpdump \
  debpkg/usr/bin/lpmake \
  debpkg/usr/bin/lpunpack \
  debpkg/usr/bin/lpadd \
  debpkg/usr/bin/lpflash \
  debpkg/usr/bin/avbtool \
  debpkg/usr/bin/ext2simg \
  debpkg/usr/bin/e2fsdroid \
  debpkg/usr/bin/mke2fs.android \
  debpkg/usr/bin/make_f2fs \
  debpkg/usr/bin/sload_f2fs \
  2>/dev/null || true)"
DEPS="${DEPS#shlibs:Depends=}"

# Add hard deps: adb, fastboot, simg2img (from Debian), python3 for scripts.
DEPS="${DEPS:+$DEPS, }adb (>= 1:34), fastboot (>= 1:34), android-sdk-libsparse-utils (>= 1:10), python3"
echo "Computed Depends: $DEPS"

cat > /build/out/debpkg/DEBIAN/control << EOF
Package: android-tools
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: amd64
Maintainer: Debian 13 build <root@localhost>
Depends: ${DEPS}
Recommends: android-udev-rules
Homepage: https://github.com/nmeum/android-tools
Description: Android platform tools (extra utilities beyond adb/fastboot)
 Extra Android platform utilities built from the nmeum/android-tools
 project: lpdump, lpmake, lpunpack, avbtool, mkbootimg,
 make_f2fs, e2fsdroid, and more.
 .
 This package provides tools not shipped by Debian's adb/fastboot packages.
 adb, fastboot, and simg2img are provided by Debian's own packages
 (dependency).
EOF

cat > /build/out/debpkg/usr/share/doc/android-tools/changelog.Debian << CL
android-tools (${VERSION}) trixie; urgency=medium

  * Initial build: extra Android platform tools from nmeum/android-tools v${VERSION}.

 -- Debian 13 build <root@localhost>  $(date -R)
CL
gzip -9n /build/out/debpkg/usr/share/doc/android-tools/changelog.Debian

log "Building .deb"
mkdir -p /build/out/debs
cd /build/out
dpkg-deb --build --root-owner-group debpkg "android-tools_${VERSION}_amd64.deb"
cp -f "android-tools_${VERSION}_amd64.deb" /build/out/debs/

log "Verifying"
dpkg-deb --info /build/out/debs/"android-tools_${VERSION}_amd64.deb" >/dev/null
echo "DONE: /build/out/debs/android-tools_${VERSION}_amd64.deb"
