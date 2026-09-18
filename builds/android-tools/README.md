# android-tools

Extra Android platform utilities for Debian 13 (trixie), built from
[nmeum/android-tools](https://github.com/nmeum/android-tools) v37.0.0.

Debian ships `adb`, `fastboot`, and `simg2img` but omits the rest. This
package fills the gap without replacing distro packages.

## Install

```bash
wget -c -O /tmp/android-tools_37.0.0_amd64.deb https://github.com/kunshakolime/trixie-tricks/releases/download/android-tools-37.0.0/android-tools_37.0.0_amd64.deb && sudo apt install /tmp/android-tools_37.0.0_amd64.deb
```

## Build

Requires `podman`.

```bash
./build-android-tools-deb.sh                     # incremental
./build-android-tools-deb.sh --reimage           # rebuild container
./build-android-tools-deb.sh --recompile         # clean build
./build-android-tools-deb.sh --reimage --recompile
```

## Included tools

| Tool | Purpose |
|---|---|
| `lpdump`, `lpmake`, `lpunpack`, `lpadd`, `lpflash` | Logical partition management |
| `avbtool` | Android Verified Boot signing |
| `mkbootimg`, `unpack_bootimg`, `repack_bootimg` | Boot image creation/extraction |
| `mkdtboimg` | Device Tree Blob Overlay images |
| `ext2simg`, `e2fsdroid`, `mke2fs.android` | ext4 Android filesystem tools |
| `make_f2fs`, `sload_f2fs` | F2FS filesystem tools |

Debian provides `adb`, `fastboot`, `simg2img`, `img2simg`, `append2simg` as dependencies.

## Extra APK tools (not in our package)

```bash
sudo apt install unzip zip apksigner zipalign apktool
# jadx: https://github.com/skylot/jadx/releases
```

## super.img workflow

```bash
simg2img super.img super.raw        # only if sparse
lpdump super.raw                    # partition offsets
lpunpack super.raw out/             # extracts system.img, vendor.img, ...
sudo mount -o loop system.img /mnt/x
```

## APK workflow

```bash
adb pull /system/priv-app/X/X.apk
adb install X.apk
adb logcat -c && adb logcat -v threadtime
apktool d X.apk → edit smali → apktool b → zipalign → apksigner sign
```

## Minimal APK build (native, required)

Shared base for native and Flutter. JDK 21 and `curl/unzip/zip` already on trixie. System-wide so all users share one install.

```bash
sudo mkdir -p /opt/android-sdk/cmdline-tools
wget -c https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip -O /tmp/tools.zip # ~150MB, resumable
sudo unzip /tmp/tools.zip -d /opt/android-sdk/cmdline-tools # ~350MB extracted
sudo mv /opt/android-sdk/cmdline-tools/cmdline-tools /opt/android-sdk/cmdline-tools/latest
echo 'export ANDROID_HOME=/opt/android-sdk' | sudo tee /etc/profile.d/android.sh
echo 'export PATH="$PATH:/opt/android-sdk/cmdline-tools/latest/bin:/opt/android-sdk/platform-tools"' | sudo tee -a /etc/profile.d/android.sh
export ANDROID_HOME=/opt/android-sdk
export PATH="$PATH:/opt/android-sdk/cmdline-tools/latest/bin:/opt/android-sdk/platform-tools"

sudo sdkmanager --sdk_root=/opt/android-sdk "platforms;android-36" "build-tools;36.0.0" "platform-tools" # ~65MB + ~55MB + ~15MB
yes | sdkmanager --sdk_root=/opt/android-sdk --licenses
```

## Flutter (optional, reuses SDK above)

Only needed for cross-platform Dart apps. Native APKs stay in dex/smali (patchable); Flutter compiles to `libapp.so` (not `baksmali`-patchable).

```bash
wget -c https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_3.47.4-stable.tar.xz -O /tmp/flutter.tar.xz # ~1.4GB, resumable
sudo tar -xf /tmp/flutter.tar.xz -C /opt # ~2.8GB extracted to /opt/flutter
echo 'export PATH="$PATH:/opt/flutter/bin"' | sudo tee /etc/profile.d/flutter.sh
export PATH="$PATH:/opt/flutter/bin"
flutter precache --android # ~1GB artifacts

flutter config --android-sdk /opt/android-sdk
yes | flutter doctor --android-licenses
flutter doctor # first run fetches Gradle ~500MB to ~/.gradle

flutter create ~/myapp # <5MB
flutter build apk --debug # ~25MB APK
adb install -r build/app/outputs/flutter-apk/app-debug.apk # ~25MB transfer
```
