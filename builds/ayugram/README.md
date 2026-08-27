# ayugram

AyuGram Desktop for Debian 13 (trixie), built from
[AyuGram/AyuGramDesktop](https://github.com/AyuGram/AyuGramDesktop) v7.0.9.

AyuGram is a Telegram Desktop fork with extra features including anti-recall,
message translation, and more.

## Install

```bash
sudo apt install ./ayugram_7.0.9_amd64.deb
```

## Build

Requires `podman`.

```bash
./build-ayugram-deb.sh                     # incremental (fast)
./build-ayugram-deb.sh --reimage           # rebuild container image
./build-ayugram-deb.sh --recompile         # clean build
./build-ayugram-deb.sh --reimage --recompile
```

## How it works

The build uses the upstream's pre-built CentOS Docker image
(`ghcr.io/telegramdesktop/tdesktop/centos_env:latest`) which contains all
dependencies (Qt 6, FFmpeg, WebRTC, etc.) pre-compiled. The AyuGram source
is cloned, built inside this container, and the resulting binary is packaged
as a Debian .deb.

## API credentials

The build uses test API credentials by default. To use your own:

Edit `build-inside.sh` and replace the `-DTDESKTOP_API_ID` and
`-DTDESKTOP_API_HASH` values. You can obtain credentials at
https://my.telegram.org.

## Notes

- The binary is statically linked against most dependencies, so the .deb has
  minimal runtime dependencies.
- Build time: ~30-60 minutes depending on hardware and cache state.
- The build cache is stored at `/var/tmp/ayugram-deb13-build` by default.
  Set `AYUGRAM_BUILD_CACHE` to override.
