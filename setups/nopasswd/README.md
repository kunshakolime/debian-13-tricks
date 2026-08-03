# nopasswd — toggle passwordless sudo

Flips passwordless sudo for the current user on/off. No arguments, each run
toggles. Writes a validated drop-in to `/etc/sudoers.d` (via `visudo -cf` +
temp file), so sudoers is never left half-written.

## Install (one-liner, system-wide)

```bash
sudo curl -fsSL https://raw.githubusercontent.com/kunshakolime/debian-13-tricks/main/setups/nopasswd/nopasswd.sh -o /usr/local/bin/nopasswd && sudo chmod +x /usr/local/bin/nopasswd && nopasswd
```

Installs `nopasswd` to `/usr/local/bin` so any user can toggle their own
passwordless sudo, then enables it for the current user.

## Usage

```bash
nopasswd    # OFF → ON, or ON → OFF
```

Remove with: `sudo rm /usr/local/bin/nopasswd`.
