# scrcpy

Display and control your Android device (screen mirroring) over USB or Wi-Fi.
Installed from the official static release into `/opt/scrcpy/` — no compiling,
no scattered files, uses your system `adb`.

## Install

```bash
sudo bash -c 'command -v adb >/dev/null || apt install -y adb; mkdir -p /opt/scrcpy && curl -fsSL https://github.com/Genymobile/scrcpy/releases/download/v4.1/scrcpy-linux-x86_64-v4.1.tar.gz | tar -xz --strip-components=1 -C /opt/scrcpy scrcpy-linux-x86_64-v4.1/scrcpy scrcpy-linux-x86_64-v4.1/scrcpy-server && chmod +x /opt/scrcpy/scrcpy && curl -fsSLo /opt/scrcpy/scrcpy.png https://raw.githubusercontent.com/Genymobile/scrcpy/v4.1/app/data/scrcpy.png && curl -fsSLo /opt/scrcpy/disconnected.png https://raw.githubusercontent.com/Genymobile/scrcpy/v4.1/app/data/disconnected.png && ln -sf /usr/bin/adb /opt/scrcpy/adb && ln -sf /opt/scrcpy/scrcpy /usr/local/bin/scrcpy && scrcpy --version'
```

Bump the version by replacing `v4.1` with the latest tag from
<https://github.com/Genymobile/scrcpy/releases>.

After installing: plug in the phone with USB debugging on, authorize the
computer, then run `scrcpy`.
