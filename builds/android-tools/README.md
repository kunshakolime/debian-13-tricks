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

## Flutter minimal APK build

Base deps (`curl git unzip xz-utils zip libglu1-mesa`) and JDK 21 are already on trixie. Skip Android Studio, Linux-desktop toolchain (`clang/cmake/ninja`), Chrome, emulator, NDK. System-wide paths so all users share one install.

```bash
# 1. Flutter SDK (most important, includes Dart)
sudo git clone https://github.com/flutter/flutter.git -b stable /opt/flutter # ~1.1GB
sudo chown -R root:root /opt/flutter # 0MB
echo 'export PATH="$PATH:/opt/flutter/bin"' | sudo tee /etc/profile.d/flutter.sh # 4KB
export PATH="$PATH:/opt/flutter/bin" # 0MB
flutter precache --android # ~1GB artifacts

# 2. Android SDK cmdline-tools (required to build)
sudo mkdir -p /opt/android-sdk/cmdline-tools # 0MB
curl -o /tmp/tools.zip https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip # ~150MB download
sudo unzip /tmp/tools.zip -d /opt/android-sdk/cmdline-tools # ~350MB extracted
sudo mv /opt/android-sdk/cmdline-tools/cmdline-tools /opt/android-sdk/cmdline-tools/latest # 0MB
echo 'export ANDROID_HOME=/opt/android-sdk' | sudo tee -a /etc/profile.d/flutter.sh # 4KB
echo 'export PATH="$PATH:/opt/android-sdk/cmdline-tools/latest/bin:/opt/android-sdk/platform-tools"' | sudo tee -a /etc/profile.d/flutter.sh # 4KB
export ANDROID_HOME=/opt/android-sdk # 0MB
export PATH="$PATH:/opt/android-sdk/cmdline-tools/latest/bin:/opt/android-sdk/platform-tools" # 0MB

# 3. SDK packages, most important first
sudo sdkmanager --sdk_root=/opt/android-sdk "platforms;android-36" "build-tools;36.0.0" "platform-tools" # ~65MB + ~55MB + ~15MB
flutter config --android-sdk /opt/android-sdk # 0MB
yes | flutter doctor --android-licenses # 0MB
flutter doctor # 0MB, first run downloads Gradle ~500MB to ~/.gradle

flutter create ~/myapp # <5MB
flutter build apk --debug # ~25MB APK, ~/.gradle grows to ~1GB on first build
adb install -r build/app/outputs/flutter-apk/app-debug.apk # pushes ~25MB to device
```
