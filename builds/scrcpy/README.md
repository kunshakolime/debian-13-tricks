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
