#!/usr/bin/env bash
#
# build-packet-deb.sh
#
# Builds Packet 0.6.1 for Debian 13 (trixie) inside a persistent podman
# container, producing packet_0.6.1_amd64.deb in OUTDIR.
#
# The container, apt/rustup toolchain, and the cargo build dir are all
# cached, so re-runs are incremental: only the `packet` crate is relinked.
#
#   ./build-packet-deb.sh [OUTDIR]      # OUTDIR defaults to $PWD
#   ./build-packet-deb.sh --clean       # full recompile (keeps toolchain)
#
# Cache location (override with $PACKET_BUILD_CACHE):
#   /var/tmp/packet-deb13-build
# To recompile from a fresh checkout of a different version:
#   rm -rf "$PACKET_BUILD_CACHE"/src "$PACKET_BUILD_CACHE"/out/build
#
# Patches applied (Debian 13 ships GTK 4.18 / libadwaita 1.7; upstream
# targets 4.22 / 1.9):
#   - Cargo.toml: gtk4 v4_22 -> v4_18, libadwaita v1_9 -> v1_7
#   - shortcuts-dialog.blp rewritten with Gtk.ShortcutsWindow
# Built with rustup rust (>= 1.92; trixie's 1.85 is below MSRV of the
# vendored crates). The binary links trixie's glibc/GTK and runs on stock
# Debian 13 for all users.

set -euo pipefail

VERSION="0.6.1"
RUST_TOOLCHAIN="1.97.1"
TARBALL_URL="https://github.com/nozwock/packet/releases/download/${VERSION}/packet-${VERSION}.tar.xz"
CONTAINER_IMAGE="debian:13"
CTR="packet-deb13-builder"
CACHE="${PACKET_BUILD_CACHE:-/var/tmp/packet-deb13-build}"

CLEAN=0
OUTDIR="$PWD"
for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=1 ;;
    *) OUTDIR="$arg" ;;
  esac
done

log() { printf '\n==> %s\n' "$*"; }

command -v podman >/dev/null || { echo "error: podman not found" >&2; exit 1; }

mkdir -p "$CACHE/src" "$CACHE/debs" "$OUTDIR"

if podman container exists "$CTR"; then
  log "Reusing build container '$CTR'"
  podman start "$CTR" >/dev/null 2>&1 || true
else
  log "Creating persistent build container '$CTR' (one-time)"
  podman run -d --name "$CTR" --network=host \
    -v "$CACHE":/build:Z \
    "$CONTAINER_IMAGE" sleep infinity
fi

if [ ! -f "$CACHE/src/Cargo.toml" ]; then
  log "Downloading + extracting source (one-time)"
  curl -fL -o "$CACHE/packet.tar.xz" "$TARBALL_URL"
  tar -xf "$CACHE/packet.tar.xz" -C "$CACHE/src"
fi

log "Applying Debian 13 source patches"
sed -i 's/features = \["v4_22"\]/features = ["v4_18"]/' "$CACHE/src/Cargo.toml"
sed -i 's/features = \["v1_9"\]/features = ["v1_7"]/' "$CACHE/src/Cargo.toml"
cat > "$CACHE/src/data/resources/ui/shortcuts-dialog.blp" <<'BLP'
using Gtk 4.0;

Gtk.ShortcutsWindow shortcuts_dialog {
    section-name: "shortcuts";

    Gtk.ShortcutsSection {
        section-name: "shortcuts";

        Gtk.ShortcutsGroup {
            title: C_("shortcut window", "General");

            Gtk.ShortcutsShortcut {
                title: C_("shortcut window", "Show Help");
                action-name: "win.help";
            }

            Gtk.ShortcutsShortcut {
                title: C_("shortcut window", "Open Menu");
                accelerator: "F10";
            }

            Gtk.ShortcutsShortcut {
                title: C_("shortcut window", "Open Preferences");
                action-name: "win.preferences";
            }

            Gtk.ShortcutsShortcut {
                title: C_("shortcut window", "Close or Hide Window");
                action-name: "window.close";
            }

            Gtk.ShortcutsShortcut {
                title: C_("shortcut window", "Quit");
                action-name: "app.quit";
            }
        }
    }
}
BLP

log "Writing in-container build script"
cat > "$CACHE/run.sh" <<'RUNEOF'
#!/usr/bin/env bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
export PATH=/root/.cargo/bin:$PATH

log() { printf '\n==> %s\n' "$*"; }

if [ ! -f /build/.apt-done ]; then
  log "apt: installing build dependencies (one-time)"
  apt-get update -qq
  apt-get install -y -qq \
    build-essential cargo rustc meson ninja-build pkg-config curl \
    libgtk-4-dev libadwaita-1-dev libglib2.0-dev libglib2.0-dev-bin \
    gettext libgettextpo-dev blueprint-compiler desktop-file-utils appstream \
    libdbus-1-dev libbluetooth-dev libudev-dev libssl-dev libasound2-dev \
    protobuf-compiler file dpkg-dev
  touch /build/.apt-done
else
  log "apt: cache hit"
fi

if [ ! -f /build/.rustup-done ]; then
  log "rustup: installing rust ${RUST_TOOLCHAIN} (one-time)"
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs -o /tmp/rustup.sh
  sh /tmp/rustup.sh -y --default-toolchain "${RUST_TOOLCHAIN}" --profile minimal --no-modify-path >/dev/null
  touch /build/.rustup-done
else
  log "rustup: cache hit"
fi

rustc --version

cd /build/src
if [ ! -d /build/out/build/meson-private ]; then
  log "meson: configure (prefix=/usr)"
  meson setup /build/out/build --prefix=/usr
else
  log "meson: using cached build dir"
fi

log "meson: compile (incremental)"
meson compile -C /build/out/build

log "meson: staged install"
rm -rf /build/out/stage
DESTDIR=/build/out/stage meson install -C /build/out/build --no-rebuild

log "dpkg: assembling package metadata"
mkdir -p /build/out/debpkg/DEBIAN /build/out/debpkg/usr/share/doc/packet /build/out/debian
cp -a /build/out/stage/usr/. /build/out/debpkg/usr/
cp /build/src/LICENSE /build/out/debpkg/usr/share/doc/packet/copyright

cat > /build/out/debian/control <<'CTL'
Source: packet
Section: net
Priority: optional
Maintainer: Debian 13 build <root@localhost>
Standards-Version: 4.7.0

Package: packet
Architecture: any
Description: Quick Share implementation for Linux
CTL

DEPS="$(cd /build/out && dpkg-shlibdeps -O debpkg/usr/bin/packet 2>/dev/null)"
DEPS="${DEPS#shlibs:Depends=}"
[ -n "$DEPS" ] || { echo "error: dpkg-shlibdeps returned nothing" >&2; exit 1; }
echo "Computed Depends: $DEPS"

cat > /build/out/debpkg/DEBIAN/postinst <<'PI'
#!/bin/sh
set -e
if [ -x /usr/bin/glib-compile-schemas ]; then
  glib-compile-schemas /usr/share/glib-2.0/schemas >/dev/null 2>&1 || true
fi
if [ -x /usr/bin/gtk4-update-icon-cache ] && [ -d /usr/share/icons/hicolor ]; then
  gtk4-update-icon-cache -f /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi
if [ -x /usr/bin/update-desktop-database ]; then
  update-desktop-database /usr/share/applications >/dev/null 2>&1 || true
fi
exit 0
PI
chmod 755 /build/out/debpkg/DEBIAN/postinst

cat > /build/out/debpkg/usr/share/doc/packet/changelog.Debian <<'CL'
packet (0.6.1) trixie; urgency=medium

  * Build for Debian 13 (trixie): patched to GTK 4.18 / libadwaita 1.7.

 -- Debian 13 build <root@localhost>  Sat, 02 Aug 2026 00:00:00 +0000
CL

cat > /build/out/debpkg/DEBIAN/control <<EOF
Package: packet
Version: ${VERSION}
Section: net
Priority: optional
Architecture: amd64
Maintainer: Debian 13 build <root@localhost>
Depends: ${DEPS}
Recommends: python3-nautilus, python3-dbus, bluez
Homepage: https://github.com/nozwock/packet
Description: Quick Share implementation for Linux
 A partial implementation of Google's Quick Share protocol that lets you
 send and receive files wirelessly from Android devices using Quick Share,
 or another device with Packet.
 .
 Requires Bluetooth to be enabled and devices connected to a Wi-Fi network
 with mDNS.
EOF

log "dpkg: building packet_${VERSION}_amd64.deb"
cd /build/out
dpkg-deb --build --root-owner-group debpkg "packet_${VERSION}_amd64.deb"
cp -f "packet_${VERSION}_amd64.deb" /build/debs/

log "verifying shared library resolution"
ldd /build/out/debpkg/usr/bin/packet | grep -q "not found" \
  && { echo "error: unresolved libs" >&2; exit 1; } || echo "all libs resolved"
dpkg-deb --info /build/debs/"packet_${VERSION}_amd64.deb" >/dev/null
echo "DONE: /build/debs/packet_${VERSION}_amd64.deb"
RUNEOF

if [ "$CLEAN" -eq 1 ]; then
  log "Wiping cached build dir (full recompile)"
  podman exec "$CTR" bash -c 'rm -rf /build/out/build'
fi

log "Running build"
podman exec -e VERSION="$VERSION" -e RUST_TOOLCHAIN="$RUST_TOOLCHAIN" \
  "$CTR" bash /build/run.sh

install -m 0644 "$CACHE/debs/packet_${VERSION}_amd64.deb" "$OUTDIR/"
echo
echo "Built: $OUTDIR/packet_${VERSION}_amd64.deb"
echo "Install on Debian 13:  sudo apt install ./packet_${VERSION}_amd64.deb"
echo "Cache kept at:         $CACHE (reuse for fast re-runs)"
