#!/usr/bin/env bash
# run.sh — in-container build for the GTK4 Miracast sink.
#
# Runs inside the persistent podman container. /build is the host cache dir,
# /container is this repo's container/ dir. Steps are incremental; nothing
# is redone unless its output is missing.
#
# Stage 1: MiracleCast backend (miracle-wifid, miracle-sinkctl, ...)
# Stage 2: GTK4 sink app (miracast-sink)

set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

log() { printf '\n==> %s\n' "$*"; }

# Install Chromecast deps if missing (avoids full image rebuild)
if ! pkg-config --exists avahi-client 2>/dev/null; then
  log "Installing Chromecast dependencies"
  apt-get update -qq
  apt-get install -y --no-install-recommends \
    libavahi-client-dev libavahi-glib-dev \
    libprotobuf-c-dev protobuf-compiler \
    libjson-glib-dev
fi

# ---------------------------------------------------------------- MiracleCast
if [ ! -d /build/out/build-miracle ]; then
  log "meson: configure miraclecast (prefix=/usr)"
  meson setup /build/out/build-miracle /build/miraclecast \
    --prefix=/usr -Denable-systemd=true
else
  log "meson: miraclecast build dir cached"
fi

log "meson: compile miraclecast (incremental)"
meson compile -C /build/out/build-miracle

log "meson: staged install miraclecast"
rm -rf /build/out/stage
DESTDIR=/build/out/stage meson install -C /build/out/build-miracle --no-rebuild

# -------------------------------------------------------- protobuf code gen
log "protoc: generating cast_channel.pb-c.h/c"
mkdir -p /appsrc/backend/chromecast
protoc --proto_path=/appsrc/backend/chromecast \
       --c_out=/appsrc/backend/chromecast \
       /appsrc/backend/chromecast/cast_channel.proto

# ------------------------------------------------------------------- GTK4 app
if [ ! -d /build/out/build-app ]; then
  log "meson: configure miracast-sink app"
  meson setup /build/out/build-app /appsrc --prefix=/usr
else
  log "meson: app build dir cached"
fi

log "meson: compile app (incremental)"
meson compile -C /build/out/build-app

log "meson: staged install app"
DESTDIR=/build/out/stage meson install -C /build/out/build-app --no-rebuild

# ------------------------------------------------------------ dpkg assembly
log "dpkg: assembling package metadata"
mkdir -p /build/out/debpkg/DEBIAN /build/out/debpkg/usr/share/doc/miracast-sink /build/out/debian

cp -a /build/out/stage/etc/. /build/out/debpkg/etc/ 2>/dev/null || true
cp -a /build/out/stage/usr/. /build/out/debpkg/usr/

# Meson installs to /usr/lib/<arch>/ but systemd reads from /usr/lib/.
if [ -d /build/out/debpkg/usr/lib/x86_64-linux-gnu/systemd ]; then
  mkdir -p /build/out/debpkg/usr/lib/systemd
  cp -a /build/out/debpkg/usr/lib/x86_64-linux-gnu/systemd/. \
         /build/out/debpkg/usr/lib/systemd/
  rm -rf /build/out/debpkg/usr/lib/x86_64-linux-gnu/systemd
fi

cat > /build/out/debian/control <<'CTL'
Source: miracast-sink
Section: net
Priority: optional
Maintainer: Debian 13 build <root@localhost>
Standards-Version: 4.7.0

Package: miracast-sink
Architecture: any
Description: GTK4 Miracast sink for Linux
CTL

DEPS="$(cd /build/out && dpkg-shlibdeps -O \
  debpkg/usr/bin/miracle-wifid \
  debpkg/usr/bin/miracle-sinkctl \
  debpkg/usr/bin/miracle-wifictl \
  debpkg/usr/bin/miracle-uibcctl \
  debpkg/usr/bin/miracast-sink 2>/dev/null)"
DEPS="${DEPS#shlibs:Depends=}"
[ -n "$DEPS" ] || { echo "error: dpkg-shlibdeps returned nothing" >&2; exit 1; }
echo "Computed Depends: $DEPS"

cat > /build/out/debpkg/DEBIAN/postinst <<'PI'
#!/bin/sh
set -e
# Enable the MiracleCast wifi daemon so the app can drive the sink.
if [ -x /usr/bin/systemctl ]; then
  systemctl daemon-reload >/dev/null 2>&1 || true
  systemctl enable miracast-wifid.service >/dev/null 2>&1 || true
fi
if [ -x /usr/bin/update-desktop-database ]; then
  update-desktop-database /usr/share/applications >/dev/null 2>&1 || true
fi
exit 0
PI
chmod 755 /build/out/debpkg/DEBIAN/postinst

cat > /build/out/debpkg/usr/share/doc/miracast-sink/changelog.Debian <<CL
miracast-sink (${VERSION}) trixie; urgency=medium

  * Initial build: MiracleCast backend for the GTK4 Miracast sink.

 -- Debian 13 build <root@localhost>  Thu, 20 Aug 2026 00:00:00 +0000
CL

cat > /build/out/debpkg/DEBIAN/control <<EOF
Package: miracast-sink
Version: ${VERSION}
Section: net
Priority: optional
Architecture: amd64
Maintainer: Debian 13 build <root@localhost>
Depends: ${DEPS}, wpasupplicant, gstreamer1.0-plugins-good, gstreamer1.0-plugins-bad, gstreamer1.0-libav
Recommends: network-manager
Homepage: https://github.com/albfan/miraclecast
Description: GTK4 Miracast sink for Linux
  Turns the machine into a Miracast receiver. Other devices can mirror
  their screen to this computer over Wi-Fi Direct.
  .
  Backend: MiracleCast (miracle-wifid + miracle-sinkctl). Frontend: GTK4
  (miracast-sink).
EOF

log "dpkg: building miracast-sink_${VERSION}_amd64.deb"
mkdir -p /build/out/debs
cd /build/out
dpkg-deb --build --root-owner-group debpkg "miracast-sink_${VERSION}_amd64.deb"
cp -f "miracast-sink_${VERSION}_amd64.deb" /build/out/debs/

log "verifying shared library resolution"
ldd /build/out/debpkg/usr/bin/miracle-* | grep -q "not found" \
  && { echo "error: unresolved libs" >&2; exit 1; } || echo "all libs resolved"
dpkg-deb --info /build/out/debs/"miracast-sink_${VERSION}_amd64.deb" >/dev/null
echo "DONE: /build/out/debs/miracast-sink_${VERSION}_amd64.deb"