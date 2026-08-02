#!/usr/bin/env bash
#
# setup-sshm-askpass.sh
#
# Sets up password auto-fill for sshm-managed SSH hosts on Debian 13.
#
# sshm (https://github.com/Gu1llaum-3/sshm) manages hosts from ~/.ssh/config
# and shells out to plain `ssh`. When a host only has password auth, ssh shows
# an interactive prompt. This setup wires up SSH_ASKPASS so ssh reads the
# password from ~/.ssh/sshm_passwords instead of asking.
#
# What it installs:
#   ~/.ssh/askpass.sh       the SSH_ASKPASS helper (extracts user@host from
#                           the prompt and greps the password file; also
#                           resolves sshm aliases -> HostName)
#   ~/.ssh/sshm_passwords   "host:password" entries (chmod 600)
#   ~/.bashrc               exports SSH_ASKPASS and SSH_ASKPASS_REQUIRE=force
#
# Usage:
#   ./setup-sshm-askpass.sh
#
# After running, reload the environment and add your passwords:
#   source ~/.bashrc
#   vim ~/.ssh/sshm_passwords   # e.g. 217.154.181.178:secret
#
# The password file is never overwritten once it exists.

set -euo pipefail

ASKPASS="$HOME/.ssh/askpass.sh"
PASSFILE="$HOME/.ssh/sshm_passwords"
BASHRC="$HOME/.bashrc"

mkdir -p "$HOME/.ssh"
chmod 700 "$HOME/.ssh"

log() { printf '\n==> %s\n' "$*"; }

cat > "$ASKPASS" <<'SCRIPT'
#!/bin/bash
# SSH_ASKPASS helper for sshm-managed hosts.
# ssh passes a prompt like "root@217.154.181.178's password: " as $1.
# Looks up a matching entry in ~/.ssh/sshm_passwords and prints the password.
#
# ~/.ssh/sshm_passwords format (one per line, '#' for comments):
#   host:password
#   user@host:password       (optional; user is also checked)
#   alias:password           (any Host alias that maps to the host also works)

PROMPT="$1"
PASSFILE="$HOME/.ssh/sshm_passwords"

[ -r "$PASSFILE" ] || exit 1

PAIR=$(printf '%s\n' "$PROMPT" | grep -m1 -oP '[\w.\-]+@[\w.\-]+')
[ -n "$PAIR" ] || exit 1

USER=${PAIR%%@*}
HOST=${PAIR##*@}

# ssh prints the resolved HostName; also collect any alias mapping to it
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

# No entry found — fall back to an interactive prompt on the controlling TTY.
# askpass's stdout is a pipe to ssh, but /dev/tty is still available when
# ssh runs from a terminal, so we can type the password (echo suppressed).
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
echo "Done. Reload and add passwords:"
echo "  source ~/.bashrc"
echo "  vim ~/.ssh/sshm_passwords"
echo
echo "Note: hosts not in sshm_passwords fall back to an interactive"
echo "password prompt and can be saved to the file on the spot."
