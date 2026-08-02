# Packet

A partial implementation of Google's Quick Share protocol — send/receive files
wirelessly with Android Quick Share devices.

## Install

```bash
sudo apt install ./packet_0.6.1_amd64.deb
```

## Rebuild

Reproducible Debian 13 build via podman. First run ~5 min, re-runs ~10 s.

```bash
./build-packet-deb.sh [OUTDIR]   # --clean for a full recompile
```
