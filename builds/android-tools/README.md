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
sudo apt install unzip zip apksigner zipalign apktool aapt
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
echo 'export PATH="$PATH:/opt/android-sdk/cmdline-tools/latest/bin"' | sudo tee -a /etc/profile.d/android.sh
export ANDROID_HOME=/opt/android-sdk
export PATH="$PATH:/opt/android-sdk/cmdline-tools/latest/bin" # sdkmanager only; adb comes from apt, no SDK platform-tools needed

sudo /opt/android-sdk/cmdline-tools/latest/bin/sdkmanager --sdk_root=/opt/android-sdk "platforms;android-36" "build-tools;36.0.0" # ~65MB + ~55MB; only android.jar + d8 used from it, rest comes from apt
yes | sudo /opt/android-sdk/cmdline-tools/latest/bin/sdkmanager --sdk_root=/opt/android-sdk --licenses
```

### First APK (manual, no Gradle)

`zipalign`/`apksigner`/`adb` come from apt — no SDK duplicates. From the SDK: `android.jar` + `d8` + `aapt2` (system `aapt2` 2.19 can't read API-36 resources, SDK `aapt2` required).

Create these 3 files in your text editor:

`~/hello/AndroidManifest.xml`
```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.example.hello">
  <application android:label="Hello">
    <activity android:name=".MainActivity" android:exported="true">
      <intent-filter><action android:name="android.intent.action.MAIN"/>
      <category android:name="android.intent.category.LAUNCHER"/></intent-filter>
    </activity>
  </application>
</manifest>
```

`~/hello/src/com/example/hello/MainActivity.java`
```java
package com.example.hello;
import android.app.Activity; import android.os.Bundle; import android.widget.TextView;
public class MainActivity extends Activity {
  protected void onCreate(Bundle b) { super.onCreate(b); TextView t = new TextView(this); t.setText("Hello"); setContentView(t); }
}
```

`~/hello/res/values/strings.xml`
```xml
<resources><string name="app_name">Hello</string></resources>
```

Then build, one step at a time (literal paths, no variables to lose between terminals):

```bash
mkdir -p ~/hello/{dex,classes,gen}
/opt/android-sdk/build-tools/36.0.0/aapt2 compile --dir ~/hello/res -o ~/hello/compiled.zip
/opt/android-sdk/build-tools/36.0.0/aapt2 link -o ~/hello/base.apk --manifest ~/hello/AndroidManifest.xml -I /opt/android-sdk/platforms/android-36/android.jar --java ~/hello/gen --min-sdk-version 24 ~/hello/compiled.zip
javac -cp /opt/android-sdk/platforms/android-36/android.jar -d ~/hello/classes $(find ~/hello/src ~/hello/gen -name '*.java')
/opt/android-sdk/build-tools/36.0.0/d8 --lib /opt/android-sdk/platforms/android-36/android.jar --output ~/hello/dex $(find ~/hello/classes -name '*.class')
(cd ~/hello && zip -j base.apk dex/classes.dex)
zipalign -f -p 4 ~/hello/base.apk ~/hello/hello.apk
keytool -genkey -keystore ~/.android/debug.keystore -alias androiddebugkey -storepass android -keypass android -keyalg RSA -validity 10000 -dname "CN=Android Debug,O=Android,C=US" # once
apksigner sign --ks ~/.android/debug.keystore --ks-pass pass:android --key-pass pass:android ~/hello/hello.apk
adb install -r ~/hello/hello.apk
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
