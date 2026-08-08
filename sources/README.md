# Debian 13 (trixie) apt sources

## Setup

```bash
sudo ./setup-sources.sh
```

Opt-in repos are written with `Enabled: no`, then `apt-sources` runs so you
pick what to enable. Trixie is skipped if already configured (stock Debian 13
ships `debian.sources`); otherwise installed as deb822 `debian.sources`.
Configs are backed up once (`.debian-tricks.bak`).

Toggle any source later with `apt-sources` (../setups/apt-sources):

```bash
sudo apt-sources                 # interactive
sudo apt-sources list            # state
sudo apt-sources enable forky    # on
```

## Repos

| Repo      | File                     | Install                                                        |
|-----------|--------------------------|----------------------------------------------------------------|
| trixie    | `debian.sources`         | base system (skipped if present)                               |
| forky     | `forky.sources`          | `sudo apt -t forky install <pkg>`                              |
| google    | `google-chrome.sources`  | `sudo apt install google-chrome-stable`                        |
| wine      | `wine.sources`           | `sudo dpkg --add-architecture i386 && sudo apt install --install-recommends wine-devel` |
| vscode    | `vscode.sources`         | `sudo apt install code`                                        |
| firefox   | `firefox.sources`        | `sudo apt install firefox`                                     |
| virtualbox| `virtualbox.sources`     | `sudo apt search virtualbox`                                   |

forky is pinned priority 100 — nothing is pulled in automatically.
Signing keys live in `/etc/apt/keyrings/`.
