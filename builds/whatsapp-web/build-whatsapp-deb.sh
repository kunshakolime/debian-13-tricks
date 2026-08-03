#!/usr/bin/env bash
set -euo pipefail

# Build a pake desktop app (.deb only) inside a Debian 13 container.
# Usage: build-whatsapp-deb.sh [OUTDIR]
#   URL and NAME are pinned below; OUTDIR defaults to this script's directory.
#
# Reuses the stopped container `pake-deb13` if present, otherwise provisions a
# fresh one from debian:13. The container is started for the build and stopped
# again afterwards. The .deb is written as
# <name>_<YYYY-MM-DD>_amd64.deb (build date instead of app version 1.0.0).

URL="https://web.whatsapp.com"
NAME="whatsapp-web"
OUT="${1:-$(cd "$(dirname "$0")" && pwd)}"

mkdir -p "$OUT"

IMAGE="docker.io/library/debian:13"
CONTAINER="pake-deb13"

stop_container() {
    podman stop "$CONTAINER" >/dev/null 2>&1 || true
}
trap stop_container EXIT

if ! podman image exists "$IMAGE"; then
    echo "[*] pulling $IMAGE ..."
    podman pull "$IMAGE"
fi

if ! podman container exists "$CONTAINER"; then
    echo "[*] creating + provisioning $CONTAINER ..."
    podman run -d --network host --name "$CONTAINER" "$IMAGE" sleep infinity
    podman exec "$CONTAINER" bash -lc 'DEBIAN_FRONTEND=noninteractive apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq build-essential pkg-config libwebkit2gtk-4.1-dev libgtk-3-dev libayatana-appindicator3-dev librsvg2-dev libssl-dev libxdo-dev curl wget file xdg-utils nodejs npm git ca-certificates'
    podman exec "$CONTAINER" bash -lc 'export SHELL=/bin/bash && npm install -g pnpm@10.26.2 && pnpm config set global-bin-dir /usr/local/bin && pnpm install -g pake-cli'
    podman exec "$CONTAINER" bash -lc 'curl --proto "=https" --tlsv1.2 -sSf https://sh.rustup.rs -o /tmp/rustup.sh && sh /tmp/rustup.sh -y --default-toolchain stable --profile minimal'
    echo "[*] provisioned $CONTAINER"
fi

if ! podman ps --format '{{.Names}}' | grep -qx "$CONTAINER"; then
    echo "[*] starting $CONTAINER ..."
    podman start "$CONTAINER"
fi

echo "[*] building .deb for $URL (name: $NAME) against Debian 13 ..."
podman exec "$CONTAINER" bash -lc "export PATH=/root/.cargo/bin:\$PATH && pake '$URL' --name '$NAME' --targets deb"

DATE="$(date +%Y-%m-%d)"
podman cp "$CONTAINER:/$NAME.deb" "$OUT/${NAME}_${DATE}_amd64.deb"
stop_container
trap - EXIT

echo "✔ built: $OUT/${NAME}_${DATE}_amd64.deb (container $CONTAINER stopped again)"
