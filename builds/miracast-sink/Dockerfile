# Dockerfile — build environment for the GTK4 Miracast sink.
#
# Debian 13 (trixie) with everything needed to compile:
#   - MiracleCast (miracle-wifid, miracle-sinkctl, miracle-wifictl, miracle-uibcctl)
#   - the GTK4 sink app (gtk4 + libadwaita + GStreamer)
#
# The image is rebuilt rarely; the per-build work happens in the persistent
# container via container/run.sh.

FROM docker.io/library/debian:13

RUN apt-get update -qq \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
    build-essential meson ninja-build pkg-config git ca-certificates curl file \
    libglib2.0-dev libsystemd-dev libudev-dev libreadline-dev \
    libgtk-4-dev libadwaita-1-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav \
    blueprint-compiler desktop-file-utils appstream gettext dpkg-dev \
 && rm -rf /var/lib/apt/lists/*