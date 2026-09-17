# LazyVim — latest Neovim in /opt

Debian 13's `neovim` is old (0.10.x vs upstream 0.12.x) and Neovim publishes no
`.deb`, so we grab the official release tarball into `/opt` and symlink it onto
PATH, then install LazyVim on top.

## Neovim → /opt

```bash
wget -c -O /tmp/nvim.tar.gz https://github.com/neovim/neovim/releases/latest/download/nvim-linux-x86_64.tar.gz
sudo rm -rf /opt/nvim
sudo mkdir -p /opt/nvim
sudo tar -C /opt/nvim --strip-components=1 -xzf /tmp/nvim.tar.gz
sudo ln -sf /opt/nvim/bin/nvim /usr/local/bin/nvim
```

LazyVim needs `git` (plugin clones) and benefits from `ripgrep` (Telescope
search); install them if missing:

```bash
sudo apt install -y git curl ripgrep
```

## LazyVim

Moves an existing config aside, then clones the starter:

```bash
mv -T ~/.config/nvim ~/.config/nvim.bak 2>/dev/null
git clone --depth 1 https://github.com/LazyVim/starter ~/.config/nvim
rm -rf ~/.config/nvim/.git
nvim
```

The first `nvim` run auto-installs all plugins; subsequent runs are instant.

To update Neovim, redo the `sudo` steps above (`wget …` skips an existing
`/tmp/nvim.tar.gz` unless you delete it first, or use `-O` with a fresh name).