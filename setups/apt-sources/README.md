# apt-sources — enable/disable any apt source

```bash
sudo install -m 755 apt-sources.sh /usr/local/bin/apt-sources
```

Works with one-line (`sources.list`, `*.list`) and deb822 (`*.sources`) files.
Sources are named by file basename without extension (`sources.list` for the
main file).

```bash
apt-sources                 # interactive checklist
apt-sources list            # show state
apt-sources enable forky    # on
apt-sources disable wine    # off
apt-sources toggle vscode   # flip
```

deb822: sets `Enabled:` (added if missing). One-line: comments/uncomments
`deb` lines (`deb-src` untouched). First change per file is backed up as
`<file>.apt-sources.bak`. Run `apt-get update` after.
