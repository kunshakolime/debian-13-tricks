# scrcpy

Display and control your Android device (screen mirroring) over USB or Wi-Fi.

## Install

```bash
sudo apt install ./scrcpy_4.1_amd64.deb
```

Pulls Debian's `adb` package as a dependency. The adb bundled in the official
static release is stripped.

After installing: plug in the phone with USB debugging on, authorize the
computer, then run `scrcpy`.

## Rebuild

Packages the official static release with a standard FHS layout
(`/usr/bin`, `/usr/share/man`, `/usr/share/doc`).

```bash
./build-scrcpy-deb.sh [OUTDIR]
```

Bump `VERSION` in `build-scrcpy-deb.sh` to track the latest tag from
<https://github.com/Genymobile/scrcpy/releases>.

## Alternative: official release tarball

Installs the official static release into `/opt/scrcpy/` — no compiling, no
scattered files, uses your system `adb`.

```bash
sudo bash -c 'command -v adb >/dev/null || apt install -y adb; mkdir -p /opt/scrcpy && curl -fsSL https://github.com/Genymobile/scrcpy/releases/download/v4.1/scrcpy-linux-x86_64-v4.1.tar.gz | tar -xz --strip-components=1 -C /opt/scrcpy scrcpy-linux-x86_64-v4.1/scrcpy scrcpy-linux-x86_64-v4.1/scrcpy-server && chmod +x /opt/scrcpy/scrcpy && curl -fsSLo /opt/scrcpy/scrcpy.png https://raw.githubusercontent.com/Genymobile/scrcpy/v4.1/app/data/scrcpy.png && curl -fsSLo /opt/scrcpy/disconnected.png https://raw.githubusercontent.com/Genymobile/scrcpy/v4.1/app/data/disconnected.png && ln -sf /usr/bin/adb /opt/scrcpy/adb && ln -sf /opt/scrcpy/scrcpy /usr/local/bin/scrcpy && scrcpy --version'
```
