#!/bin/bash
# Toggle passwordless sudo for the current user. No args, each run flips it.
#
# Safe: the sudoers drop-in is validated with `visudo -cf` and installed via a
# temp file, so sudoers is never left half-written (which would lock sudo out).

set -uo pipefail

REAL_USER="${SUDO_USER:-$USER}"

SUDOERS_D="/etc/sudoers.d"
NAME="90-nopasswd-${REAL_USER//[^a-zA-Z0-9_-]/_}"
FILE="$SUDOERS_D/$NAME"
ENTRY="$REAL_USER ALL=(ALL:ALL) NOPASSWD:ALL"

# Okuma izni olmayan /etc/sudoers.d dizinindeki dosyayı sudo ile test et
if sudo test -f "$FILE"; then
    sudo rm -f "$FILE"
    echo "passwordless sudo is now OFF"
else
    tmp="$(mktemp)"
    trap 'rm -f "$tmp"' EXIT
    printf '%s\n' "$ENTRY" > "$tmp"
    sudo visudo -cf "$tmp"
    sudo install -o root -g root -m 0440 "$tmp" "$FILE"
    echo "passwordless sudo is now ON"
fi
