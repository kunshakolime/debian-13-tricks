# sshm password auto-fill

Auto-fills passwords for [sshm](https://github.com/Gu1llaum-3/sshm) hosts via
ssh's `SSH_ASKPASS`. Unknown hosts prompt on the TTY and offer to save.

## Setup (Linux/macOS)

```bash
curl -fsSL https://raw.githubusercontent.com/kunshakolime/debian-13-tricks/main/setups/sshm-askpass/setup-sshm-askpass.sh | bash
```

Installs (idempotent): `~/.ssh/askpass.sh`, a `host:password` file
`~/.ssh/sshm_passwords`, and `SSH_ASKPASS` exports in `~/.bashrc`.

## Setup (Termux/Android)

```bash
curl -fsSL https://raw.githubusercontent.com/kunshakolime/debian-13-tricks/main/setups/sshm-askpass/setup-sshm-askpass-termux.sh | bash
```

Termux differs from a regular Linux box, so the Termux script also:

- installs the **OpenSSH** client first (Termux's default `ssh` is Dropbear,
  which ignores `SSH_ASKPASS`) and removes dropbear if it conflicts;
- downloads the **sshm** binary from the GitHub releases (arch-aware), since
  there is no Termux package for it;
- adds `StrictHostKeyChecking accept-new` to `~/.ssh/config` so the first
  connection to a new host does not die with "Host key verification failed".

Installs (idempotent): `openssh`, the `sshm` binary, `~/.ssh/askpass.sh`,
`~/.ssh/sshm_passwords`, `SSH_ASKPASS` exports in `~/.bashrc`, and
`StrictHostKeyChecking accept-new` in `~/.ssh/config`.

## Passwords

```bash
vim ~/.ssh/sshm_passwords
```

```
217.154.181.178:superSecret
root@217.154.181.178:rootPassword
uknown-spain-32G:anotherPassword
```

Keys checked in order: `user@host`, `host`, then any alias resolving to that
hostname (ssh prints the resolved `HostName`, not the alias).

## Caveats

- No `:` in passwords (file is `:`-separated).
- No TTY (cron, GUI launcher) → unknown hosts still fail auth.
