# Dockerfile — build environment for AyuGramDesktop (Debian 13).
#
# Wraps the upstream tdesktop CentOS build image which has all
# dependencies (Qt 6, FFmpeg, WebRTC, etc.) pre-built.

FROM ghcr.io/telegramdesktop/tdesktop/centos_env:latest

USER root

RUN dnf install -y protobuf-devel ccache && dnf clean all \
 && cd /tmp \
 && curl -sLO https://github.com/protocolbuffers/protobuf/releases/download/v21.12/protobuf-cpp-3.21.12.tar.gz \
 && tar xf protobuf-cpp-3.21.12.tar.gz \
 && cd protobuf-3.21.12 \
 && cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -Dprotobuf_BUILD_TESTS=OFF \
    -DCMAKE_PREFIX_PATH=/usr/local \
 && cmake --build build -j$(nproc) \
 && cmake --install build \
 && ldconfig \
 && rm -rf /tmp/protobuf*
