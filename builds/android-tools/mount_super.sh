#!/usr/bin/env bash
#
# mount_super.sh — mount every logical partition inside an Android super.img
# using device-mapper, without extracting/copying partition data.
#
# Requirements:
#   - android-tools package (provides lpdump, simg2img)
#   - dmsetup, losetup, mount (usually pre-installed)
#   - root/sudo
#   - real host with loop device support (not containers without --privileged)
#
# Usage:
#   sudo ./mount_super.sh /path/to/super.img [/mount/base/dir]
#
# Unmount everything it created:
#   sudo ./mount_super.sh --umount

set -euo pipefail

MOUNT_BASE="${2:-/mnt/super_partitions}"
STATE_FILE="/tmp/mount_super.state"
MOUNT_SUCCESS=0

require_root() {
    if [[ $EUID -ne 0 ]]; then
        echo "Run as root (sudo)." >&2
        exit 1
    fi
}

cleanup_on_error() {
    local rc=$?
    [[ $rc -eq 0 || $MOUNT_SUCCESS -eq 1 ]] && return
    echo >&2
    echo "Error (exit $rc) — cleaning up partial mounts..." >&2
    if [[ -f "$STATE_FILE" ]]; then
        while read -r name mp; do
            mountpoint -q "$mp" 2>/dev/null && umount "$mp" 2>/dev/null || true
            dmsetup info "$name" &>/dev/null && dmsetup remove "$name" 2>/dev/null || true
        done < "$STATE_FILE"
        local loop_dev
        loop_dev=$(grep '^LOOP=' "$STATE_FILE" | head -1 | cut -d= -f2)
        if [[ -n "$loop_dev" ]] && /usr/sbin/losetup "$loop_dev" &>/dev/null; then
            /usr/sbin/losetup -d "$loop_dev" 2>/dev/null || true
        fi
        rm -f "$STATE_FILE"
    fi
    rm -rf "$WORKDIR"
    echo "Cleaned up." >&2
}

cleanup_all() {
    require_root
    if [[ ! -f "$STATE_FILE" ]]; then
        echo "No state file found ($STATE_FILE) — nothing to clean up."
        exit 0
    fi
    # shellcheck disable=SC2162
    while read -r name mp; do
        if mountpoint -q "$mp" 2>/dev/null; then
            echo "Unmounting $mp"
            umount "$mp" || echo "  warn: failed to unmount $mp"
        fi
        if dmsetup info "$name" &>/dev/null; then
            echo "Removing dm device $name"
            dmsetup remove "$name" || echo "  warn: failed to remove $name"
        fi
    done < "$STATE_FILE"

    LOOP_DEV=$(grep '^LOOP=' "$STATE_FILE" | cut -d= -f2 || true)
    if [[ -n "${LOOP_DEV:-}" ]] && losetup "$LOOP_DEV" &>/dev/null; then
        echo "Detaching $LOOP_DEV"
        losetup -d "$LOOP_DEV"
    fi

    rm -f "$STATE_FILE"
    echo "Done."
    exit 0
}

if [[ "${1:-}" == "--umount" ]]; then
    cleanup_all
fi

IMG="${1:-}"
if [[ -z "$IMG" || ! -f "$IMG" ]]; then
    echo "Usage: $0 /path/to/super.img [/mount/base/dir]" >&2
    echo "       $0 --umount [/mount/base/dir]" >&2
    exit 1
fi

require_root
trap cleanup_on_error EXIT
for bin in lpdump dmsetup losetup mount; do
    command -v "$bin" >/dev/null || { echo "Missing required tool: $bin" >&2; exit 1; }
done

> "$STATE_FILE"
mkdir -p "$MOUNT_BASE"
WORKDIR=$(mktemp -d)

RAW_IMG="$IMG"

# --- Step 1: sparse -> raw if needed ---
# Detect sparse images and tell the user to convert beforehand.
# Sparse images must be converted: simg2img super.img super.raw
IS_SPARSE=0
if command -v file >/dev/null 2>&1; then
    if file "$IMG" 2>/dev/null | grep -qi "sparse image"; then
        IS_SPARSE=1
    fi
fi

if [[ "$IS_SPARSE" -eq 1 ]]; then
    echo "Error: image is sparse. Convert it first: simg2img $IMG ${IMG%.img}.raw" >&2
    exit 1
fi
echo "[1/5] Image is raw."

# --- Step 2: attach loop device ---
echo "[2/5] Attaching loop device..."
LOOP_DEV=$(losetup -fP --show "$RAW_IMG")
echo "      -> $LOOP_DEV"
echo "LOOP=$LOOP_DEV" >> "$STATE_FILE"

# --- Step 3: dump metadata ---
echo "[3/5] Reading partition metadata..."
LPDUMP_OUT=$(lpdump "$LOOP_DEV" 2>/dev/null || lpdump "$RAW_IMG")

# Reject multi-device super (spans multiple block devices)
NUM_DEVICES=$(echo "$LPDUMP_OUT" | grep -c 'Partition name:' || true)
if [[ "$NUM_DEVICES" -gt 1 ]]; then
    echo "Error: multi-device super detected ($NUM_DEVICES block devices)." >&2
    echo "This script only supports single-device super images." >&2
    exit 1
fi

# --- Step 4/5: parse each partition's extents and map + mount ---
# lpdump text output looks like:
#   Name: system_a
#   ...
#   Extents:
#     0 .. 1048575 linear super 2048
#
# Format: "<start> .. <end> linear <device> <target_offset>"
# dmsetup table: "0 <num_sectors> linear <loop_dev> <target_offset>"
echo "$LPDUMP_OUT" | awk '
    /^[[:space:]]*Name:/ { name=$2 }
    /linear/ && !/Partition|layout/ {
        n = split($0, parts, " ")
        start = parts[1]
        end = parts[3]
        target = parts[6]
        len = end - start + 1
        print name, len, target
    }
' > "$WORKDIR/partitions"

while read -r NAME LEN TARGET_OFFSET; do
    [[ -z "$NAME" ]] && continue
    echo "[4/5] Mapping $NAME (length=$LEN sectors, target_offset=$TARGET_OFFSET)"
    DM_NAME="super_${NAME}"

    if dmsetup info "$DM_NAME" &>/dev/null; then
        echo "      already exists, skipping create"
    else
        dmsetup create "$DM_NAME" --table "0 $LEN linear $LOOP_DEV $TARGET_OFFSET"
    fi

    MP="$MOUNT_BASE/$NAME"
    mkdir -p "$MP"

    # Idempotency: skip if already mounted at this path
    if mountpoint -q "$MP" 2>/dev/null; then
        echo "      $NAME already mounted at $MP, skipping"
        echo "$DM_NAME $MP" >> "$STATE_FILE"
        continue
    fi

    echo "[5/5] Mounting $NAME -> $MP"
    if mount -o ro "/dev/mapper/$DM_NAME" "$MP" 2>/dev/null; then
        echo "$DM_NAME $MP" >> "$STATE_FILE"
    else
        echo "      warn: mount failed for $NAME (may not be a filesystem, e.g. an empty/reserved region)"
        dmsetup remove "$DM_NAME" || true
    fi
done

rm -rf "$WORKDIR"
MOUNT_SUCCESS=1
echo
echo "All done. Partitions mounted under: $MOUNT_BASE"
echo "To clean up later: sudo $0 --umount $MOUNT_BASE"
