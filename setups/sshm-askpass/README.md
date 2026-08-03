# sshm password auto-fill

Auto-fills passwords for [sshm](https://github.com/Gu1llaum-3/sshm) hosts via
ssh's `SSH_ASKPASS`. Unknown hosts prompt on the TTY and offer to save.

## Setup

```bash
curl -fsSL https://raw.githubusercontent.com/kunshakolime/debian-13-tricks/main/setups/sshm-askpass/setup-sshm-askpass.sh | bash
```

Installs (idempotent): `~/.ssh/askpass.sh`, a `host:password` file
`~/.ssh/sshm_passwords`, and `SSH_ASKPASS` exports in `~/.bashrc`.

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
