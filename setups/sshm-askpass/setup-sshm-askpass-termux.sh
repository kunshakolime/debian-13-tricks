#!/data/data/com.termux/files/usr/bin/bash
#
# setup-sshm-askpass-termux.sh
#
# Termux version of the sshm password auto-fill setup.
#
# Differs from the Debian script:
#   * Termux's default ssh client is Dropbear, which ignores SSH_ASKPASS,
#     so OpenSSH is installed first (dropbear removed if it conflicts).
#   * sshm is a Go binary with no Termux package, so it is downloaded from
#     the GitHub releases (arch-aware).
#   * askpass.sh handles host key verification prompts (returns "yes")
#     so first connects to new hosts do not fail with
#     "Host key verification failed" (ssh would otherwise try to prompt
#     on a TTY that is not available).
#   * Termux auto-sources ~/.bashrc for every new session, so the SSH_ASKPASS
#     exports apply without manual sourcing.
#
# What it installs (idempotent):
#   openssh                      OpenSSH client (replaces dropbear 'ssh')
#   ~/.local or $PREFIX/bin/sshm the sshm binary
#   ~/.ssh/askpass.sh            the SSH_ASKPASS helper
#   ~/.ssh/sshm_passwords        "host:password" entries (chmod 600)
#   ~/.bashrc                    SSH_ASKPASS + SSH_ASKPASS_REQUIRE=force
#
# Usage:
#   ./setup-sshm-askpass-termux.sh
#
# After running, add your passwords and reload the current session:
#   source ~/.bashrc
#   vim ~/.ssh/sshm_passwords

set -euo pipefail

log() { printf '\n==> %s\n' "$*"; }

if [ -z "${PREFIX:-}" ] || ! [ -d "$PREFIX" ]; then
    echo "This script is for Termux (missing \$PREFIX)." >&2
    exit 1
fi

# ---------- 1) OpenSSH client (SSH_ASKPASS requires it) ----------
if command -v ssh >/dev/null 2>&1 && ssh -V 2>&1 | grep -qi openssh; then
    log "OpenSSH already installed: $(ssh -V 2>&1)"
else
    if pkg list-installed 2>/dev/null | grep -q '^dropbear/'; then
        log "Removing dropbear (its 'ssh' conflicts with openssh)..."
        pkg remove -y dropbear
    fi
    log "Installing openssh..."
    pkg install -y openssh
    log "OpenSSH installed: $(ssh -V 2>&1)"
fi

# ---------- 2) sshm binary ----------
if command -v sshm >/dev/null 2>&1 && [ -x "$(command -v sshm)" ]; then
    log "sshm already installed: $(sshm --version 2>/dev/null || echo 'unknown version')"
else
    case "$(uname -m)" in
        aarch64|arm64) ARCH="arm64" ;;
        x86_64)        ARCH="amd64" ;;
        armv7*|armv8l) ARCH="armv7" ;;
        i686)          ARCH="386" ;;
        *) echo "Unsupported arch: $(uname -m)"; exit 1 ;;
    esac
    URL="https://github.com/Gu1llaum-3/sshm/releases/latest/download/sshm_Linux_${ARCH}.tar.gz"
    TMP="$(mktemp -d)"
    log "Downloading sshm (${ARCH})..."
    curl -sSL "$URL" -o "$TMP/sshm.tar.gz"
    tar -xzf "$TMP/sshm.tar.gz" -C "$TMP"
    install -m 755 "$TMP/sshm" "$PREFIX/bin/sshm"
    rm -rf "$TMP"
    log "sshm installed: $(sshm --version)"
fi

# ---------- 3) SSH_ASKPASS helper ----------
ASKPASS="$HOME/.ssh/askpass.sh"
PASSFILE="$HOME/.ssh/sshm_passwords"
BASHRC="$HOME/.bashrc"

mkdir -p "$HOME/.ssh"
chmod 700 "$HOME/.ssh"

cat > "$ASKPASS" <<'SCRIPT'
#!/data/data/com.termux/files/usr/bin/bash
PROMPT="$1"
PASSFILE="$HOME/.ssh/sshm_passwords"

# Handle host key verification prompts (e.g. "Are you sure you want to
# continue connecting (yes/no)?"). SSH_ASKPASS_REQUIRE=force means ssh
# sends these through askpass as well; without this, they fail with
# "Host key verification failed".
case "$PROMPT" in
    *"continue connecting"*|*"yes/no"*)
        printf 'yes\n'
        exit 0
        ;;
esac

[ -r "$PASSFILE" ] || exit 1

PAIR=$(printf '%s\n' "$PROMPT" | grep -m1 -oP '[\w.\-]+@[\w.\-]+')
[ -n "$PAIR" ] || exit 1

USER=${PAIR%%@*}
HOST=${PAIR##*@}

CANDIDATES=$(awk -v h="$HOST" '
    tolower($1)=="host"  { a=$2 }
    tolower($1)=="hostname" && tolower($2)==tolower(h) { print a }
' "$HOME/.ssh/config" 2>/dev/null)

for key in $HOST $CANDIDATES; do
    if [ -n "$USER" ]; then
        PW=$(grep -m1 -E "^${USER}@${key}:" "$PASSFILE" | cut -d: -f2-)
        [ -n "$PW" ] && { printf '%s\n' "$PW"; exit 0; }
    fi
    PW=$(grep -m1 -E "^${key}:" "$PASSFILE" | cut -d: -f2-)
    [ -n "$PW" ] && { printf '%s\n' "$PW"; exit 0; }
done

if { exec 3<>/dev/tty; } 2>/dev/null; then
    printf '%s' "$PROMPT" >&3
    IFS= read -rs PW_INPUT <&3
    printf '\n' >&3

    case "$PW_INPUT" in
        *:*)
            printf 'not saving: password contains ":"\n' >&3
            ;;
        "")
            : ;;
        *)
            printf 'Save this password to ~/.ssh/sshm_passwords? [y] ' >&3
            IFS= read -r ANSWER <&3
            printf '\n' >&3
            case "$ANSWER" in
                ""|[Yy]*)
                    [ -f "$PASSFILE" ] || : > "$PASSFILE"
                    if [ -w "$PASSFILE" ]; then
                        printf '%s\n' "${USER}@${HOST}:${PW_INPUT}" >> "$PASSFILE"
                        printf 'Saved %s@%s for future connections\n' "$USER" "$HOST" >&3
                    else
                        printf 'cannot write %s\n' "$PASSFILE" >&3
                    fi
                    ;;
            esac
            ;;
    esac

    printf '%s\n' "$PW_INPUT"
    exit 0
fi

exit 1
SCRIPT
chmod 700 "$ASKPASS"
log "Wrote $ASKPASS"

if [ ! -f "$PASSFILE" ]; then
    cat > "$PASSFILE" <<'TEMPLATE'
# Passwords for sshm-managed hosts, one per line.
# Format: <host>:<password>  (or <user>@<host>:<password> for per-user)
# Use the hostname, the sshm alias, or the IP ssh actually connects to.
#
# Example:
# 217.154.181.178:superSecret
# root@217.154.181.178:rootPassword
# uknown-spain-32G:anotherPassword
TEMPLATE
    log "Wrote $PASSFILE (template)"
else
    log "Kept existing $PASSFILE"
fi
chmod 600 "$PASSFILE"

if [ -f "$BASHRC" ] && grep -q 'SSH_ASKPASS_REQUIRE=force' "$BASHRC"; then
    log "Env exports already present in $BASHRC"
else
    printf '\n# Use askpass helper so sshm hosts can auto-supply passwords\nSSH_ASKPASS="$HOME/.ssh/askpass.sh"\nexport SSH_ASKPASS\nexport SSH_ASKPASS_REQUIRE=force\n' >> "$BASHRC"
    log "Added env exports to $BASHRC"
fi

echo
echo "Done."
echo "  openssh:       $(ssh -V 2>&1)"
echo "  sshm:          $(command -v sshm) ($(sshm --version 2>/dev/null | cut -d' ' -f1-3))"
echo "  askpass:       $ASKPASS"
echo "  passwords:     $PASSFILE"
echo
echo "New Termux sessions pick this up automatically."
echo "For the CURRENT session, run once:  source ~/.bashrc"
echo "Then add passwords:                 vim ~/.ssh/sshm_passwords"
