#!/usr/bin/env bash
#
# apt-sources — list, enable, disable, and toggle apt sources.
#
# Works with every installed source:
#   /etc/apt/sources.list              (legacy one-line format)
#   /etc/apt/sources.list.d/*.list     (legacy one-line format)
#   /etc/apt/sources.list.d/*.sources  (deb822 format)
#
# Sources are referred to by name — the file basename without its extension,
# or "sources.list" for the main file (e.g. "forky", "google-chrome").
#
# Usage:
#   apt-sources                 interactive pick (whiptail checklist)
#   apt-sources list            show every source and its state
#   apt-sources enable NAME...  enable source(s)
#   apt-sources disable NAME... disable source(s)
#   apt-sources toggle NAME...  flip source(s)
#
# The first change to a file is backed up as <file>.apt-sources.bak.
# Run apt-get update after changing sources.

set -euo pipefail

SOURCES_LIST=/etc/apt/sources.list
SOURCE_D=/etc/apt/sources.list.d
BAK_SUFFIX=.apt-sources.bak

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

# Print discovered sources as: file<TAB>name<TAB>format
discover() {
    if [ -f "$SOURCES_LIST" ]; then
        printf '%s\t%s\t%s\n' "$SOURCES_LIST" "sources.list" "one-line"
    fi
    for f in "$SOURCE_D"/*.sources "$SOURCE_D"/*.list; do
        [ -f "$f" ] || continue
        local name
        name=${f##*/}
        case "$f" in
            *.sources) printf '%s\t%s\t%s\n' "$f" "${name%.sources}" "deb822" ;;
            *.list)    printf '%s\t%s\t%s\n' "$f" "${name%.list}" "one-line" ;;
        esac
    done
}

# Resolve a source name to "file format"; exit 1 if unknown.
resolve() {
    local name="$1"
    if [ "$name" = "sources.list" ] && [ -f "$SOURCES_LIST" ]; then
        printf '%s %s\n' "$SOURCES_LIST" one-line
        return 0
    fi
    if [ -f "$SOURCE_D/$name.sources" ]; then
        printf '%s %s\n' "$SOURCE_D/$name.sources" deb822
    elif [ -f "$SOURCE_D/$name.list" ]; then
        printf '%s %s\n' "$SOURCE_D/$name.list" one-line
    else
        return 1
    fi
}

# Enabled? 0 = enabled, 1 = disabled.
is_enabled() {
    local file="$1" fmt="$2"
    if [ "$fmt" = deb822 ]; then
        case "$(sed -nE 's/^[[:space:]]*Enabled:[[:space:]]*//p' "$file" | head -1)" in
            ""|yes|true|1) return 0 ;;
            *) return 1 ;;
        esac
    else
        grep -qE '^deb[[:space:]]' "$file"
    fi
}

backup() {
    local file="$1"
    if [ -f "$file" ] && [ ! -f "$file$BAK_SUFFIX" ]; then
        cp -a "$file" "$file$BAK_SUFFIX"
    fi
}

# set_enabled <file> <fmt> yes|no
set_enabled() {
    local file="$1" fmt="$2" val="$3"
    [ -f "$file" ] || return 1
    backup "$file"
    if [ "$fmt" = deb822 ]; then
        if grep -qE '^[[:space:]]*Enabled:' "$file"; then
            sed -i -E "s/^([[:space:]]*Enabled:).*/\1 $val/" "$file"
        else
            sed -i "1i Enabled: $val" "$file"
        fi
    else
        if [ "$val" = yes ]; then
            sed -i -E 's/^#[[:space:]]*deb[[:space:]]/deb /' "$file"
        else
            sed -i -E 's/^deb[[:space:]]/# deb /' "$file"
        fi
    fi
}

set_named() {
    local name="$1" val="$2" file fmt
    read -r file fmt < <(resolve "$name") || { echo "No source named '$name' (see: apt-sources list)"; return 1; }
    set_enabled "$file" "$fmt" "$val"
    log "$name: $([ "$val" = yes ] && echo enabled || echo disabled) ($file)"
}

list_sources() {
    local file name fmt state
    printf '%-16s %-9s %s\n' "NAME" "STATE" "FILE"
    while read -r file name fmt; do
        if is_enabled "$file" "$fmt"; then state=enabled; else state=disabled; fi
        printf '%-16s %-9s %s\n' "$name" "$state" "$file"
    done < <(discover)
}

# Is $1 an exact member of the remaining args?
contains() {
    local needle="$1" hay
    shift
    for hay in "$@"; do
        [ "$hay" = "$needle" ] && return 0
    done
    return 1
}

interactive_whiptail() {
    local count=0 args=() file name fmt state item sel
    while read -r file name fmt; do
        if is_enabled "$file" "$fmt"; then state=ON; else state=OFF; fi
        args+=("$name" "$file" "$state")
        count=$((count + 1))
    done < <(discover)
    [ "$count" -eq 0 ] && { echo "No apt sources found."; return 0; }
    sel=$(whiptail --title "apt sources" \
        --checklist "Enable / disable repositories (Space = toggle, Enter = done)" \
        16 80 "$count" "${args[@]}" 3>&1 1>&2 2>&3) || return 1
    local wanted=() item
    for item in $sel; do wanted+=("${item//\"/}"); done
    while read -r file name fmt; do
        if contains "$name" "${wanted[@]}"; then
            set_enabled "$file" "$fmt" yes
            log "Enabled $name"
        else
            set_enabled "$file" "$fmt" no
            log "Disabled $name"
        fi
    done < <(discover)
}

interactive_plain() {
    local sources=() file name fmt ans i
    while read -r file name fmt; do
        sources+=("$file" "$name" "$fmt")
    done < <(discover)
    for ((i = 0; i < ${#sources[@]}; i += 3)); do
        file=${sources[i]}; name=${sources[i+1]}; fmt=${sources[i+2]}
        if is_enabled "$file" "$fmt"; then
            read -r -p "Disable '$name'? [y/N] " ans
            case "$ans" in y|Y) set_enabled "$file" "$fmt" no; log "Disabled $name" ;; esac
        else
            read -r -p "Enable '$name'? [y/N] " ans
            case "$ans" in y|Y) set_enabled "$file" "$fmt" yes; log "Enabled $name" ;; esac
        fi
    done
}

interactive() {
    if command -v whiptail >/dev/null 2>&1; then
        interactive_whiptail || { echo "Cancelled - nothing changed."; return 1; }
    else
        echo "whiptail not found, using plain prompts:"
        interactive_plain
    fi
    echo
    echo "Run: apt-get update"
}

usage() {
    sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'
}

main() {
    ensure_root "$@"
    local cmd="${1:-pick}"
    [ "$#" -gt 0 ] && shift
    case "$cmd" in
        list|ls|status) list_sources ;;
        enable)  for n in "$@"; do set_named "$n" yes || true; done ;;
        disable) for n in "$@"; do set_named "$n" no  || true; done ;;
        toggle)  for n in "$@"; do
                     local file fmt
                     read -r file fmt < <(resolve "$n") || { echo "No source named '$n' (see: apt-sources list)"; continue; }
                     if is_enabled "$file" "$fmt"; then
                         set_enabled "$file" "$fmt" no
                         log "$n: disabled"
                     else
                         set_enabled "$file" "$fmt" yes
                         log "$n: enabled"
                     fi
                 done ;;
        pick|interactive) interactive ;;
        -h|--help|help) usage ;;
        *) echo "Unknown command: $cmd"; echo; usage; exit 1 ;;
    esac
}

main "$@"
