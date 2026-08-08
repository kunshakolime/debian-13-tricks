#!/usr/bin/env bash
#
# setup-deb-get.sh
#
# Installs deb-get (https://github.com/wimpysworld/deb-get) — apt-get
# functionality for .debs published via GitHub Releases or direct download.
#
# Use it to install/update apps that only ship .debs on GitHub releases
# (Obsidian, VSCodium, GitHub Desktop, ...). Example:
#   deb-get update
#   deb-get install obsidian
#   deb-get upgrade
#
# GitHub's unauthenticated API is rate-limited (60 calls/hour per IP), which
# deb-get shares across all its repos. If you hit it, set a token:
#   echo 'export GH_TOKEN=github_pat_...' >> ~/.bashrc && source ~/.bashrc
#
# Usage:
#   ./setup-deb-get.sh

set -euo pipefail

log() { printf '\n==> %s\n' "$*"; }

if [ "$(id -u)" -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        exec sudo "$0" "$@"
    fi
    echo "Must run as root (or via sudo)." >&2
    exit 1
fi

if command -v deb-get >/dev/null 2>&1; then
    log "deb-get already installed: $(command -v deb-get)"
    echo "  update its index:  deb-get update"
    echo "  install an app:    deb-get install <app>"
    echo "  upgrade installed: deb-get upgrade"
    exit 0
fi

log "Installing prerequisites (curl, wget, jq, lsb-release, distro-info-data)..."
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y curl wget jq lsb-release distro-info-data

log "Installing deb-get via the official installer..."
curl -sL https://raw.githubusercontent.com/wimpysworld/deb-get/main/deb-get \
    | bash -s install deb-get

if command -v deb-get >/dev/null 2>&1; then
    log "deb-get installed."
    echo
    echo "First run:"
    echo "  sudo deb-get update"
    echo
    echo "Then, e.g.:"
    echo "  sudo deb-get search <app>       # search available apps"
    echo "  sudo deb-get install <app>      # install latest .deb from GitHub"
    echo "  sudo deb-get upgrade            # update all installed deb-get apps"
    echo
    echo "Tip: set a GitHub token (GH_TOKEN) in ~/.bashrc if you hit the"
    echo "60 requests/hour API rate limit."
else
    echo "deb-get install failed - check the installer output above." >&2
    exit 1
fi
