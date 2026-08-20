# GTK4 Miracast sink

Turn the machine into a Miracast receiver with a GTK4 frontend.

## Status

Building and packaging on Debian 13. The `.deb` now ships the MiracleCast
backend (`miracle-wifid`, ...) **and** the native GTK4 app (`miracast-sink`).

## Architecture

```
miracast-sink  (GTK4/Adwaita, C)
├── dbus-wifi.c   GDBus client → org.freedesktop.miracle.wifi (system bus)
│                 enumerate links, P2PScanning, auto-accept GoNegRequest,
│                 watch Connected property + signals
├── rtsp-wfd.c    WFD RTSP client: sink dials out to source:7236, answers
│                 OPTIONS/GET_PARAMETER/SET_PARAMETER, drives SETUP/PLAY
├── stream.c      GStreamer: udpsrc 7236 ! rtpmp2tdepay ! tsdemux ! h264parse
│                 ! avdec_h264 ! videoconvert ! gtk4paintablesink → GtkPicture
├── window.c      Adwaita window: status bar, device label, video viewport
└── main.c        Wiring + lifecycle
```

miracle-wifid runs as a root systemd service; the app talks to it over the
system bus.

## Layout

| Path | Purpose |
|---|---|
| `src/` | GTK4 app (meson project) |
| `Dockerfile` | Build environment (apt deps: meson, GTK4, GStreamer, systemd-dev, ...) |
| `build-miracast-deb.sh` | Orchestrator: podman container lifecycle, cache, deb output |
| `container/run.sh` | In-container build: MiracleCast + app + dpkg assembly |

## Build

```bash
./build-miracast-deb.sh [OUTDIR]   # OUTDIR defaults to $PWD
./build-miracast-deb.sh --clean    # full recompile (keeps image + source)
./build-miracast-deb.sh --image    # rebuild the Dockerfile image, then build
```

Cache lives at `/var/tmp/miracast-deb13-build` (override with `$MIRACAST_BUILD_CACHE`).

## Install

```bash
sudo apt install ./miracast-sink_0.1.0_amd64.deb
```

## Planned

- systemd unit + widened D-Bus policy (packaging)
- Audio (AAC) path
- Discovery list UI, PIN/approval prompts
- Chromecast backend behind the same frontend