# android-tools

Extra Android platform utilities for Debian 13 (trixie), built from
[nmeum/android-tools](https://github.com/nmeum/android-tools) v37.0.0.

Debian ships `adb`, `fastboot`, and `simg2img` but omits the rest. This
package fills the gap without replacing distro packages.

## Install

```bash
sudo apt install ./android-tools_37.0.0_amd64.deb
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
