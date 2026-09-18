# LazyVim — latest Neovim in /opt

Debian 13's `neovim` is old (0.10.x vs upstream 0.12.x) and has no official
`.deb`, so install the release tarball into `/opt`, then LazyVim on top.

## Neovim

```bash
wget -c -O /tmp/nvim.tar.gz https://github.com/neovim/neovim/releases/latest/download/nvim-linux-x86_64.tar.gz
sudo rm -rf /opt/nvim
sudo mkdir -p /opt/nvim
sudo tar -C /opt/nvim --strip-components=1 -xzf /tmp/nvim.tar.gz
sudo ln -sf /opt/nvim/bin/nvim /usr/local/bin/nvim
```

To update: redo the `sudo` lines above.

## Dependencies

```bash
sudo apt install -y git curl ripgrep build-essential unzip
```

`git` = plugin clones, `ripgrep` = telescope search, `build-essential` =
tree-sitter parsers (compiled with gcc/make), `unzip` = parser download.

## Icons (Nerd Font)

LazyVim icons render as □ without a Nerd Font. Install one globally:

```bash
sudo mkdir -p /usr/local/share/fonts
wget -c -O /tmp/JetBrainsMono.zip https://github.com/ryanoasis/nerd-fonts/releases/latest/download/JetBrainsMono.zip
sudo unzip -o /tmp/JetBrainsMono.zip -d /usr/local/share/fonts
sudo fc-cache -f
```

Then set your terminal font to `JetBrainsMono Nerd Font Mono` (enable `Custom font` / disable `Use system font`) and reopen.

## LazyVim

```bash
mv -T ~/.config/nvim ~/.config/nvim.bak 2>/dev/null
git clone --depth 1 https://github.com/LazyVim/starter ~/.config/nvim
rm -rf ~/.config/nvim/.git
nvim
```

First `nvim` run installs all plugins.