#!/bin/bash
# mkapk.sh — scaffold a minimal no-Gradle APK project.
# Usage: ./mkapk.sh [package] [label] [dir]
# Then edit <dir>/build.sh (paths, keystore, sdk levels) and run it.
set -euo pipefail

PKG="${1:-com.example.hello}"
LABEL="${2:-Hello}"
DIR="${3:-hello}"

PKGDIR="$DIR/src/${PKG//.//}"
mkdir -p "$PKGDIR" "$DIR/res/values"

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

if [ ! -f "$DIR/build.sh" ]; then
cat > "$DIR/build.sh" <<'SH'
#!/bin/bash
# build.sh — build + debug-sign this project. Edit config below, then run.
set -euo pipefail

# --- config: edit me ---
SDK=/opt/android-sdk
PLATFORM=android-36
BUILD_TOOLS=36.0.0
MIN_SDK=24
KS="$HOME/.android/debug.keystore"
KALIAS=androiddebugkey
STOREPASS=android
KEYPASS=android
OUT=app.apk
# --- end config ---

P="$(dirname "$0")"
AAPT="$SDK/build-tools/$BUILD_TOOLS/aapt2"
D8="$SDK/build-tools/$BUILD_TOOLS/d8"
API="$SDK/platforms/$PLATFORM/android.jar"

for t in "$AAPT" "$D8" "$API" "$(command -v javac)" "$(command -v zipalign)" "$(command -v apksigner)"; do
  [ -e "$t" ] || { echo "missing: $t" >&2; exit 1; }
done

mkdir -p "$P/dex" "$P/classes" "$P/gen"
"$AAPT" compile --dir "$P/res" -o "$P/compiled.zip"
"$AAPT" link -o "$P/base.apk" --manifest "$P/AndroidManifest.xml" -I "$API" --java "$P/gen" --min-sdk-version "$MIN_SDK" "$P/compiled.zip"
# shellcheck disable=SC2046
javac -cp "$API" -d "$P/classes" $(find "$P/src" "$P/gen" -name '*.java')
# shellcheck disable=SC2046
"$D8" --lib "$API" --output "$P/dex" $(find "$P/classes" -name '*.class')
zip -j "$P/base.apk" "$P/dex/classes.dex" > /dev/null
rm -f "$P/$OUT"
zipalign -f -p 4 "$P/base.apk" "$P/$OUT"
[ -f "$KS" ] || keytool -genkey -keystore "$KS" -alias "$KALIAS" -storepass "$STOREPASS" -keypass "$KEYPASS" -keyalg RSA -validity 10000 -dname "CN=Android Debug,O=Android,C=US"
apksigner sign --ks "$KS" --ks-pass "pass:$STOREPASS" --key-pass "pass:$KEYPASS" "$P/$OUT"
apksigner verify "$P/$OUT"
echo "OK: $P/$OUT"
SH
chmod +x "$DIR/build.sh"
fi
echo "scaffolded: $DIR (edit $DIR/build.sh, then run it)"
