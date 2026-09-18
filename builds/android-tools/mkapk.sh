#!/bin/bash
# mkapk.sh — scaffold + build a minimal debug APK, no Gradle.
# Usage: ./mkapk.sh [package] [label] [dir]
#   package: Java package, default com.example.hello
#   label:   app label, default Hello
#   dir:     project dir, default ./hello (created if missing)
# Template files are written only when absent, so edits survive rebuilds.
# Output: <dir>/app.apk (debug-signed). Install: adb install -r <dir>/app.apk
set -euo pipefail

PKG="${1:-com.example.hello}"
LABEL="${2:-Hello}"
DIR="${3:-hello}"
SDK=/opt/android-sdk
AAPT="$SDK/build-tools/36.0.0/aapt2"
D8="$SDK/build-tools/36.0.0/d8"
API="$SDK/platforms/android-36/android.jar"
KS="$HOME/.android/debug.keystore"

for t in "$AAPT" "$D8" "$API" "$(command -v javac)" "$(command -v zipalign)" "$(command -v apksigner)"; do
  [ -e "$t" ] || { echo "missing: $t" >&2; exit 1; }
done

PKGDIR="$DIR/src/${PKG//.//}"
mkdir -p "$PKGDIR" "$DIR/res/values" "$DIR/dex" "$DIR/classes" "$DIR/gen"

[ -f "$DIR/AndroidManifest.xml" ] || cat > "$DIR/AndroidManifest.xml" <<XML
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="$PKG">
  <application android:label="$LABEL">
    <activity android:name=".MainActivity" android:exported="true">
      <intent-filter><action android:name="android.intent.action.MAIN"/>
      <category android:name="android.intent.category.LAUNCHER"/></intent-filter>
    </activity>
  </application>
</manifest>
XML

[ -f "$PKGDIR/MainActivity.java" ] || cat > "$PKGDIR/MainActivity.java" <<JAVA
package $PKG;
import android.app.Activity; import android.os.Bundle; import android.widget.TextView;
public class MainActivity extends Activity {
  protected void onCreate(Bundle b) { super.onCreate(b); TextView t = new TextView(this); t.setText("$LABEL"); setContentView(t); }
}
JAVA

[ -f "$DIR/res/values/strings.xml" ] || cat > "$DIR/res/values/strings.xml" <<XML
<resources><string name="app_name">$LABEL</string></resources>
XML

"$AAPT" compile --dir "$DIR/res" -o "$DIR/compiled.zip"
"$AAPT" link -o "$DIR/base.apk" --manifest "$DIR/AndroidManifest.xml" -I "$API" --java "$DIR/gen" --min-sdk-version 24 "$DIR/compiled.zip"
# shellcheck disable=SC2046
javac -cp "$API" -d "$DIR/classes" $(find "$DIR/src" "$DIR/gen" -name '*.java')
# shellcheck disable=SC2046
"$D8" --lib "$API" --output "$DIR/dex" $(find "$DIR/classes" -name '*.class')
zip -j "$DIR/base.apk" "$DIR/dex/classes.dex" > /dev/null
zipalign -f -p 4 "$DIR/base.apk" "$DIR/app.apk"
[ -f "$KS" ] || keytool -genkey -keystore "$KS" -alias androiddebugkey -storepass android -keypass android -keyalg RSA -validity 10000 -dname "CN=Android Debug,O=Android,C=US"
apksigner sign --ks "$KS" --ks-pass pass:android --key-pass pass:android "$DIR/app.apk"
apksigner verify "$DIR/app.apk"
echo "OK: $DIR/app.apk — install with: adb install -r $DIR/app.apk"
