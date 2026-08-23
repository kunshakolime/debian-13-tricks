# Dockerfile — build environment for the GTK4 Miracast sink.
#
# Debian 13 (trixie) with everything needed to compile:
#   - MiracleCast (miracle-wifid, miracle-sinkctl, miracle-wifictl, miracle-uibcctl)
#   - the GTK4 sink app (gtk4 + libadwaita + GStreamer)
#   - Chromecast support (Avahi, protobuf-c, json-glib)
#
# Layers are split by dependency group so changing one doesn't rebuild all.

FROM docker.io/library/debian:13

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential meson ninja-build ccache pkg-config git ca-certificates curl file \
 && rm -rf /var/lib/apt/lists/*

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libglib2.0-dev libsystemd-dev libudev-dev libreadline-dev \
 && rm -rf /var/lib/apt/lists/*

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libgtk-4-dev libadwaita-1-dev \
 && rm -rf /var/lib/apt/lists/*

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav \
 && rm -rf /var/lib/apt/lists/*

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libavahi-client-dev libavahi-glib-dev \
    libprotobuf-c-dev protobuf-compiler protobuf-c-compiler \
    libjson-glib-dev \
 && rm -rf /var/lib/apt/lists/*

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    blueprint-compiler desktop-file-utils appstream gettext dpkg-dev \
 && rm -rf /var/lib/apt/lists/*
