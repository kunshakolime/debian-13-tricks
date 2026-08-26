#!/bin/bash
# Quick mock test for miracast-sink. Stops real daemon, runs mock,
# lets you test the app, then cleans up.
#
# Usage: ./test-local.sh [path-to-app]

APP="${1:-/usr/bin/miracast-sink}"
DIR="$(cd "$(dirname "$0")" && pwd)"
WORK="/tmp/miracast-local-test"
MOCK_LOG="$WORK/mock.log"
APP_LOG="$WORK/app.log"
MOCK_PID=""
APP_PID=""

cleanup() {
  [ -n "$MOCK_PID" ] && kill "$MOCK_PID" 2>/dev/null
  [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null
  sudo systemctl start msk-wifid.service 2>/dev/null
}
trap cleanup EXIT

mkdir -p "$WORK"

echo "[1/4] Stopping real miracle-wifid..."
sudo systemctl stop msk-wifid.service
sleep 1

echo "[2/4] Starting mock source..."
sudo python3 "$DIR/mock-wifid.py" > "$MOCK_LOG" 2>&1 &
MOCK_PID=$!
sleep 2
grep -q "owned org.freedesktop.miracle.wifi" "$MOCK_LOG" || { echo "FAIL: mock couldn't own bus"; cat "$MOCK_LOG"; exit 1; }

echo "[3/4] Running app (you have 15s to watch it)..."
echo ""
echo "  To run manually instead, press Ctrl+C and run:"
echo "    G_MESSAGES_DEBUG=all $APP"
echo ""
echo "  The mock will connect after ~2s. You should see color bars."
echo ""

G_MESSAGES_DEBUG=all GST_DEBUG=2 \
  xvfb-run -a -s "-screen 0 800x600x24" "$APP" > "$APP_LOG" 2>&1 &
APP_PID=$!
sleep 15

echo "[4/4] Checking results..."
echo ""

check() {
  grep -q "$2" "$3" 2>/dev/null && echo "  PASS: $1" || echo "  FAIL: $1"
}

check "mock owns bus"            "owned org.freedesktop.miracle.wifi" "$MOCK_LOG"
check "GoNegRequest sent"        "emitting GoNegRequest"              "$MOCK_LOG"
check "peer accepted"            "Connect called"                     "$MOCK_LOG"
check "RTSP handshake (SETUP)"   "RTSP << SETUP"                     "$MOCK_LOG"
check "RTSP handshake (PLAY)"    "RTSP << PLAY"                      "$MOCK_LOG"
check "handshake complete"       "handshake complete"                 "$MOCK_LOG"

grep -qE "GStreamer.*CRITICAL|Internal data stream error" "$APP_LOG" 2>/dev/null \
  && echo "  FAIL: GStreamer error" || echo "  PASS: no GStreamer errors"

echo ""
echo "Logs: $WORK/"
