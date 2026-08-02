# sshm password auto-fill

Lets [sshm](https://github.com/Gu1llaum-3/sshm)-managed hosts that only use
password auth connect without typing the password each time. Uses ssh's
`SSH_ASKPASS` hook: the helper reads the prompt, looks up the password in
`~/.ssh/sshm_passwords`, and prints it.

Why this is needed: sshm shells out to plain `ssh`, and ssh shows the prompt
only when there is no way to feed it a password non-interactively.

## Setup

```bash
./setup-sshm-askpass.sh
```

Installs (all idempotent — safe to re-run):

- `~/.ssh/askpass.sh` — the askpass helper (recreated on each run)
- `~/.ssh/sshm_passwords` — `host:password` entries, chmod 600 (never overwritten)
- `~/.bashrc` — exports `SSH_ASKPASS` and `SSH_ASKPASS_REQUIRE=force`

## Add passwords

```bash
source ~/.bashrc
vim ~/.ssh/sshm_passwords
```

Format, one per line (`#` for comments):

```
217.154.181.178:superSecret
root@217.154.181.178:rootPassword
uknown-spain-32G:anotherPassword
```

Use the resolved IP/hostname, the sshm alias, or a `user@host` pair. The
helper checks `user@host`, then `host`, then any sshm alias that resolves to
that hostname (ssh prints the resolved `HostName`, not the alias, in its
prompt).

## Interactive fallback

If a host has no entry, the helper prompts for the password on the terminal
(echo suppressed) instead of failing, then asks whether to save it to
`sshm_passwords` for future connections.

## Caveats

- Passwords containing `:` can't be used (the file is `:`-separated).
- Without a terminal (cron, GUI launcher), a host not in the file still fails
  auth — there's no TTY to prompt on.
