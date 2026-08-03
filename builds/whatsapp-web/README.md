# whatsapp-web

WhatsApp Web packaged as a desktop app (built with pake / Tauri, WebKitGTK 4.1).

## Install

```bash
sudo apt install ./whatsapp-web_2026-08-03_amd64.deb
```

Apt pulls the runtime deps (`libwebkit2gtk-4.1-0`, `libgtk-3-0`,
`libayatana-appindicator3-1`, `curl`, `wget`). The .deb is named with the build
date instead of pake's default `1.0.0` version.

## Rebuild

Reproducible Debian 13 build via podman. First run ~10 min (provisions the
`pake-deb13` container: node/pnpm/pake-cli/Rust + GTK/webkit dev libs), re-runs
~1-2 min (Rust build cached). The container is left stopped after each build.

```bash
./build-whatsapp-deb.sh [OUTDIR]
```

To package a different website, change `URL` (and `NAME`) at the top of
`build-whatsapp-deb.sh`.
