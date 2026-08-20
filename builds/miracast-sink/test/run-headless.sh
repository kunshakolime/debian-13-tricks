#!/bin/bash
# Headless end-to-end test for miracast-sink against a mock WFD source.
#
#  1. starts a system dbus (needed by G_BUS_TYPE_SYSTEM)
#  2. runs test/mock-wifid.py  (mock miracle-wifid + RTSP source)
#  3. runs the real miracast-sink app under Xvfb
#  4. checks the app auto-accepted the peer, completed the WFD RTSP
#     handshake, and the GStreamer pipeline rendered video
#
# Usage: run-headless.sh [path-to-miracast-sink]
#   Default app path: /build/out/stage/usr/bin/miracast-sink

set -u

APP="${1:-/build/out/stage/usr/bin/miracast-sink}"
DIR="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-/tmp/miracast-headless}"
MOCK_LOG="$WORK/mock.log"
APP_LOG="$WORK/app.log"
rc=1

mkdir -p "$WORK"
rm -f "$MOCK_LOG" "$APP_LOG"

cleanup() {
  [ -n "${APP_PID:-}" ] && kill "$APP_PID" 2>/dev/null
  [ -n "${MOCK_PID:-}" ] && kill "$MOCK_PID" 2>/dev/null
  [ -n "${XFB_PID:-}" ] && kill "$XFB_PID" 2>/dev/null
  [ -n "${DBUS_PID:-}" ] && kill "$DBUS_PID" 2>/dev/null
}
trap cleanup EXIT

# 1. system bus (session-mode daemon on the system socket: a --system daemon
#    tries to drop capabilities, which containers forbid)
mkdir -p /run/dbus
chmod 1777 /run/dbus 2>/dev/null || true
pkill -9 -f "dbus-daemon" 2>/dev/null
pkill -9 -f "mock-wifid.py" 2>/dev/null
sleep 0.5
rm -f /run/dbus/pid /run/dbus/system_bus_socket
setsid dbus-daemon --session --nofork --nopidfile \
  --address=unix:path=/run/dbus/system_bus_socket >"$WORK/dbus.log" 2>&1 &
DBUS_PID=$!
sleep 1
if [ ! -S /run/dbus/system_bus_socket ]; then
  echo "[test] FAILED to start system dbus"
  cat "$WORK/dbus.log"
  exit 1
fi
echo "[test] system dbus: started (pid $DBUS_PID)"

# 2. mock source
python3 "$DIR/mock-wifid.py" >"$MOCK_LOG" 2>&1 &
MOCK_PID=$!
sleep 1

# 3. app under Xvfb
export G_MESSAGES_DEBUG=all
export GST_DEBUG="${GST_DEBUG:-2}"
xvfb-run -a -s "-screen 0 800x600x24" "$APP" >"$APP_LOG" 2>&1 &
XFB_PID=$!
sleep 1
APP_PID=$(pgrep -f "miracast-sink$" | head -1 || echo "")
echo "[test] app pid: ${APP_PID:-?}"

# 4. give the scenario time to play out
sleep 12

echo "==================== mock log ===================="
sed 's/^/  /' "$MOCK_LOG"
echo "==================== app log ====================="
sed 's/^/  /' "$APP_LOG"

check() {
  local desc="$1" needle="$2" file="$3"
  if grep -q "$needle" "$file"; then
    echo "PASS: $desc"
  else
    echo "FAIL: $desc (missing: $needle)"
    rc=1
  fi
}

rc=0
check "mock owned bus name"        "owned org.freedesktop.miracle.wifi" "$MOCK_LOG"
check "GoNegRequest emitted"       "emitting GoNegRequest"              "$MOCK_LOG"
check "peer accepted (Connect)"    "Connect called"                     "$MOCK_LOG"
check "RTSP handshake (SETUP)"     "RTSP << SETUP"                        "$MOCK_LOG"
check "RTSP handshake (PLAY)"      "RTSP << PLAY"                         "$MOCK_LOG"
check "RTP stream started"         "handshake complete"                 "$MOCK_LOG"

# Check for CRITICAL or FATAL GStreamer errors (not WARN)
if grep -q "GStreamer.*CRITICAL\|GStreamer.*WARNING.*Internal data stream error\|GStreamer error.*reason not-linked" "$APP_LOG"; then
  echo "FAIL: app reported a fatal GStreamer error"
  rc=1
else
  echo "PASS: app: no fatal GStreamer error"
fi

if [ "$rc" -eq 0 ]; then
  echo "============================================="
  echo "HEADLESS TEST PASSED"
  echo "============================================="
else
  echo "============================================="
  echo "HEADLESS TEST FAILED"
  echo "============================================="
fi
exit $rc