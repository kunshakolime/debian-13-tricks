# deb-get — GitHub-release .debs as packages

apt-get-style commands for apps that only publish `.deb`s on GitHub Releases
(Obsidian, VSCodium, ...).

## Install

```bash
sudo curl -fsSL https://raw.githubusercontent.com/kunshakolime/debian-13-tricks/main/setups/deb-get/setup-deb-get.sh -o /tmp/setup-deb-get.sh && sudo bash /tmp/setup-deb-get.sh
```

## Usage

```bash
sudo deb-get update          # refresh GitHub release indexes
sudo deb-get search <app>
sudo deb-get install <app>
sudo deb-get upgrade
```

If `update` hits GitHub's unauthenticated rate limit (60 req/hr), set a token:

```bash
echo 'export GH_TOKEN=github_pat_...' >> ~/.bashrc && source ~/.bashrc
```

Uninstall: `sudo apt remove --purge deb-get`
