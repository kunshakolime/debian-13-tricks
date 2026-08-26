# GTK4 Miracast / Chromecast sink

GTK4 app that turns the machine into a Miracast or Chromecast receiver.

## Status

v0.3.0. Both backends run simultaneously. Miracast is tested end-to-end.
Chromecast has discovery, TLS, protobuf, and media commands but needs a real
device test.

## Architecture

```
miracast-sink  (GTK4/Adwaita, C)
├── backend/
│   ├── msk-backend.c         MskBackend interface (start/stop/get_paintable/get_name)
│   ├── miracast/
│   │   ├── msk-miracast-backend.c   wraps dbus-wifi + rtsp-wfd
│   │   ├── dbus-wifi.c        GDBus → org.freedesktop.miracle.wifi
│   │   └── rtsp-wfd.c         WFD RTSP: sink dials source:7236
│   └── chromecast/
│       ├── msk-chromecast-backend.c   wires discovery → client → media
│       ├── discovery.c         Avahi mDNS (_googlecast._tcp)
│       ├── client.c            Cast v2 TLS, protobuf encode/decode
│       ├── media.c             LOAD/PLAY/PAUSE/SEEK/STOP + status parsing
│       └── cast_channel.proto  CastChannel message definition
├── stream.c      GStreamer pipeline → GtkPicture
├── window.c      Adwaita window
└── main.c        both backends run, first to connect wins
```

## Layout

| Path | Purpose |
|---|---|
| `src/` | GTK4 app (meson project) |
| `Dockerfile` | Build environment |
| `build-miracast-deb.sh` | Orchestrator (--reimage, --recompile) |
| `build-container.sh` | Container lifecycle (--refresh) |
| `container/run.sh` | In-container build + dpkg assembly |
| `test/` | Integration tests (need miracle-wifid) |

## Build

```bash
./build-miracast-deb.sh                # incremental
./build-miracast-deb.sh --reimage      # rebuild container + app
./build-miracast-deb.sh --recompile    # app only
```

## Install

```bash
sudo apt install ./miracast-sink_0.3.0_amd64.deb
```

## Usage

```bash
miracast-sink
```

Both Miracast and Chromecast backends start automatically.
The first device to connect becomes the active stream.

## What's next

- Test Chromecast with a real device
- Run both backends simultaneously
- Tests for Chromecast backend
- Audio (AAC) support
- Discovery list UI, device picker
