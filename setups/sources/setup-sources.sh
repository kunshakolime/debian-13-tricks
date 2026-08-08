#!/usr/bin/env bash
#
# setup-sources.sh
#
# Sets up apt sources for Debian 13 (trixie) and adds opt-in repositories
# using the modern deb822 format in /etc/apt/sources.list.d:
#
#   /etc/apt/sources.list.d/debian.sources    trixie (base system, deb822)
#   /etc/apt/sources.list.d/forky.sources     Debian 14 "forky" (testing)
#   /etc/apt/sources.list.d/google-chrome.sources    Google Chrome
#   /etc/apt/sources.list.d/wine.sources      Wine (openSUSE OBS, latest)
#   /etc/apt/sources.list.d/vscode.sources    Microsoft VSCode
#   /etc/apt/sources.list.d/firefox.sources   Mozilla Firefox
#   /etc/apt/sources.list.d/virtualbox.sources Oracle VirtualBox
#   /etc/apt/preferences.d/forky              forky pinned low (priority 100)
#
# trixie is only written when no enabled trixie source exists yet (stock Debian
# 13 installs ship debian.sources, so it's skipped); otherwise it's installed
# the modern way as a deb822 file. Everything else is written with
# `Enabled: no`, then apt-sources (../apt-sources) runs interactively so you
# can enable the repos you want. Keyrings are fetched to /etc/apt/keyrings/
# either way, ready to flip on.
#
# forky is pinned to priority 100 so nothing is pulled in automatically.
# Grab a package from it explicitly with:
#     apt -t forky install <package>
#
# After writing, apt-sources runs interactively to pick which repositories
# to enable, then apt-get update runs.
#
# Usage:
#   ./setup-sources.sh

set -euo pipefail

SOURCE_D=/etc/apt/sources.list.d
PREFS_D=/etc/apt/preferences.d
KEYRINGS_D=/etc/apt/keyrings
DEBIAN_SRC=$SOURCE_D/debian.sources
FORKY_SRC=$SOURCE_D/forky.sources
GOOGLE_SRC=$SOURCE_D/google-chrome.sources
WINE_SRC=$SOURCE_D/wine.sources
VSCODE_SRC=$SOURCE_D/vscode.sources
FIREFOX_SRC=$SOURCE_D/firefox.sources
VBOX_SRC=$SOURCE_D/virtualbox.sources
FORKY_PREF=$PREFS_D/forky
DEBIAN_KEYRING=/usr/share/keyrings/debian-archive-keyring.gpg

GOOGLE_KEYRING=$KEYRINGS_D/google-chrome.gpg
GOOGLE_KEY_URL=https://dl.google.com/linux/linux_signing_key.pub
WINE_KEYRING=$KEYRINGS_D/wine-obs.gpg
WINE_KEY_URL=https://download.opensuse.org/repositories/Emulators:/Wine:/Debian/Debian_13/Release.key
VSCODE_KEYRING=$KEYRINGS_D/microsoft.gpg
VSCODE_KEY_URL=https://packages.microsoft.com/keys/microsoft.asc
FIREFOX_KEYRING=$KEYRINGS_D/packages.mozilla.org.asc
FIREFOX_KEY_URL=https://packages.mozilla.org/apt/repo-signing-key.gpg
VBOX_KEYRING=$KEYRINGS_D/virtualbox-org.gpg
VBOX_KEY_URL=https://www.virtualbox.org/download/oracle_vbox_2016.asc
ARCH=$(dpkg --print-architecture 2>/dev/null || echo amd64)

log() { printf '\n==> %s\n' "$*"; }

ensure_root() {
    if [ "$(id -u)" -ne 0 ]; then
        if command -v sudo >/dev/null 2>&1; then
            exec sudo "$0" "$@"
        fi
        echo "Must run as root (or via sudo)." >&2
        exit 1
    fi
}

backup() {
    for f in "$DEBIAN_SRC" "$FORKY_SRC" "$GOOGLE_SRC" "$WINE_SRC" "$VSCODE_SRC" \
             "$FIREFOX_SRC" "$VBOX_SRC" "$FORKY_PREF"; do
        if [ -f "$f" ] && [ ! -f "$f.debian-tricks.bak" ]; then
            cp -a "$f" "$f.debian-tricks.bak"
            log "Backed up $f"
        fi
    done
}

# Is an *enabled* trixie source already configured (one-line or deb822)?
trixie_configured() {
    local f
    if grep -rqE '^deb[[:space:]]+https?://[^[:space:]]+[[:space:]]+trixie([[:space:]]|$)' \
            /etc/apt/sources.list /etc/apt/sources.list.d/*.list 2>/dev/null; then
        return 0
    fi
    for f in /etc/apt/sources.list.d/*.sources; do
        [ -f "$f" ] || continue
        if grep -q '^Suites:.*trixie' "$f" && ! grep -q '^Enabled: no' "$f"; then
            return 0
        fi
    done
    return 1
}

write_debian() {
    cat > "$DEBIAN_SRC" <<EOF
# Debian 13 "trixie" - base system (deb822).
Types: deb
URIs: https://deb.debian.org/debian/
Suites: trixie trixie-updates trixie-proposed-updates trixie-backports
Components: main contrib non-free non-free-firmware
Signed-By: $DEBIAN_KEYRING

Types: deb
URIs: https://security.debian.org/debian-security/
Suites: trixie-security
Components: main contrib non-free non-free-firmware
Signed-By: $DEBIAN_KEYRING
EOF
}

write_forky() {
    cat > "$FORKY_SRC" <<EOF
# Debian 14 "forky" (testing) - opt-in, low priority.
# Use explicitly with:  apt -t forky install <package>
Types: deb
URIs: https://deb.debian.org/debian/
Suites: forky
Components: main contrib non-free non-free-firmware
Architectures: $ARCH
Signed-By: $DEBIAN_KEYRING
Enabled: no
EOF
}

write_forky_pref() {
    mkdir -p "$PREFS_D"
    cat > "$FORKY_PREF" <<'EOF'
Package: *
Pin: release n=forky
Pin-Priority: 100
EOF
}

fetch_keyring() {
    local keyring="$1" url="$2" label="$3"
    [ -f "$keyring" ] && return 0
    command -v curl >/dev/null 2>&1 || { log "curl not found - cannot fetch $label keyring"; return 1; }
    command -v gpg  >/dev/null 2>&1 || { log "gpg not found - cannot fetch $label keyring"; return 1; }
    mkdir -p "$KEYRINGS_D"
    if curl -fsSL "$url" | gpg --dearmor -o "$keyring" 2>/dev/null; then
        chmod 644 "$keyring"
        log "Installed $label signing key: $keyring"
        return 0
    fi
    log "Could not fetch $label signing key from $url"
    return 1
}

fetch_keyring_armored() {
    local keyring="$1" url="$2" label="$3"
    [ -f "$keyring" ] && return 0
    command -v curl >/dev/null 2>&1 || { log "curl not found - cannot fetch $label keyring"; return 1; }
    mkdir -p "$KEYRINGS_D"
    if curl -fsSL "$url" -o "$keyring" 2>/dev/null; then
        chmod 644 "$keyring"
        log "Installed $label signing key: $keyring"
        return 0
    fi
    log "Could not fetch $label signing key from $url"
    return 1
}

write_google() {
    fetch_keyring "$GOOGLE_KEYRING" "$GOOGLE_KEY_URL" "Google" || true
    cat > "$GOOGLE_SRC" <<EOF
# Google Chrome - opt-in.
# Use explicitly with:  apt install google-chrome-stable
Types: deb
URIs: https://dl.google.com/linux/chrome/deb/
Suites: stable
Components: main
Architectures: amd64
Signed-By: $GOOGLE_KEYRING
Enabled: no
EOF
}

write_wine() {
    fetch_keyring "$WINE_KEYRING" "$WINE_KEY_URL" "Wine" || true
    cat > "$WINE_SRC" <<EOF
# Wine (openSUSE OBS Emulators:/Wine:/Debian) - opt-in, latest upstream builds.
# Install with:  apt install --install-recommends wine-devel
# (wine-staging / wine-stable also available)
Types: deb
URIs: https://download.opensuse.org/repositories/Emulators:/Wine:/Debian/Debian_13/
Suites: /
Components:
Architectures: amd64 i386
Signed-By: $WINE_KEYRING
Enabled: no
EOF
}

write_vscode() {
    fetch_keyring "$VSCODE_KEYRING" "$VSCODE_KEY_URL" "VSCode" || true
    cat > "$VSCODE_SRC" <<EOF
# Microsoft VSCode - opt-in.
# Use explicitly with:  apt install code
Types: deb
URIs: https://packages.microsoft.com/repos/code
Suites: stable
Components: main
Architectures: amd64
Signed-By: $VSCODE_KEYRING
Enabled: no
EOF
}

write_firefox() {
    fetch_keyring_armored "$FIREFOX_KEYRING" "$FIREFOX_KEY_URL" "Firefox" || true
    cat > "$FIREFOX_SRC" <<EOF
# Mozilla Firefox - opt-in.
# Use explicitly with:  apt install firefox
Types: deb
URIs: https://packages.mozilla.org/apt
Suites: mozilla
Components: main
Architectures: amd64
Signed-By: $FIREFOX_KEYRING
Enabled: no
EOF
}

write_virtualbox() {
    fetch_keyring "$VBOX_KEYRING" "$VBOX_KEY_URL" "VirtualBox" || true
    cat > "$VBOX_SRC" <<EOF
# Oracle VirtualBox - opt-in.
# Use explicitly with:  apt search virtualbox   (e.g. apt install virtualbox-7.1)
Types: deb
URIs: https://download.virtualbox.org/virtualbox/debian
Suites: trixie
Components: contrib
Architectures: amd64
Signed-By: $VBOX_KEYRING
Enabled: no
EOF
}

usage_hints() {
    echo
    echo "Sources written. To flip any of them on/off:"
    echo "  sudo apt-sources"
    echo
    echo "Installing from each repo:"
    echo "  forky     : sudo apt -t forky install <package>"
    echo "  google    : sudo apt install google-chrome-stable"
    echo "  wine      : sudo dpkg --add-architecture i386 && sudo apt update && sudo apt install --install-recommends wine-devel"
    echo "  vscode    : sudo apt install code"
    echo "  firefox   : sudo apt install firefox"
    echo "  virtualbox: sudo apt search virtualbox   # e.g. apt install virtualbox-7.1"
    echo
}

find_apt_sources() {
    if command -v apt-sources >/dev/null 2>&1; then
        command -v apt-sources
    elif [ -x "$(dirname "$0")/../apt-sources/apt-sources.sh" ]; then
        echo "$(dirname "$0")/../apt-sources/apt-sources.sh"
    else
        return 1
    fi
}

main() {
    ensure_root "$@"

    local APT_SOURCES
    APT_SOURCES=$(find_apt_sources) || {
        echo "apt-sources not found - you can enable repos manually instead:" >&2
        echo "  sudo nano /etc/apt/sources.list.d/*.sources   # flip 'Enabled: no' to 'Enabled: yes'" >&2
        echo "  sudo apt-get update" >&2
        echo "Or install and run apt-sources:" >&2
        echo "  sudo install -m 755 $(dirname "$0")/../apt-sources/apt-sources.sh /usr/local/bin/apt-sources" >&2
        exit 1
    }

    backup
    if trixie_configured; then
        log "trixie already configured - skipping"
    else
        write_debian
        log "Wrote $DEBIAN_SRC"
    fi
    write_forky
    write_forky_pref
    write_google
    write_wine
    write_vscode
    write_firefox
    write_virtualbox
    log "Wrote $FORKY_SRC and $FORKY_PREF"
    log "Wrote $GOOGLE_SRC"
    log "Wrote $WINE_SRC"
    log "Wrote $VSCODE_SRC"
    log "Wrote $FIREFOX_SRC"
    log "Wrote $VBOX_SRC"

    log "apt-sources - pick which sources to enable, then done (cancelling keeps changes)"
    "$APT_SOURCES" || true

    usage_hints

    log "Refreshing package lists..."
    apt-get update
}

main "$@"
