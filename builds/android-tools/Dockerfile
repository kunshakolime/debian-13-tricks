# Dockerfile — build environment for android-tools (Debian 13).
#
# Compiles the nmeum/android-tools project which provides lpdump, avbtool,
# mkbootimg, and other Android platform utilities not shipped by Debian.
# adb and fastboot are NOT packaged — we depend on Debian's versions.

FROM docker.io/library/debian:13

# Layer 1: package lists (cached independently)
RUN apt-get update

# Layer 2: base build tools
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build ccache pkg-config git ca-certificates curl file

# Layer 3: Go (required for adb/fastboot build — even though we don't package them)
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    golang-go

# Layer 4: libraries required by the extra tools
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libfmt-dev libbrotli-dev liblz4-dev libpcre2-dev \
    libusb-1.0-0-dev libzstd-dev zlib1g-dev \
    protobuf-compiler libprotobuf-dev

# Layer 5: gtest (needed for gtest_prod.h in vendored adb/zip_writer.h)
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libgtest-dev

# Layer 6: packaging tools + cleanup
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    dpkg-dev \
 && rm -rf /var/lib/apt/lists/*
