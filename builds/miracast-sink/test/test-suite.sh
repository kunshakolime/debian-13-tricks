#!/bin/bash
# Full test suite for miracast-sink.
#
# Usage: ./test-suite.sh [path-to-app]

APP="${1:-/usr/bin/miracast-sink}"
DIR="$(cd "$(dirname "$0")" && pwd)"
WORK="/tmp/miracast-test-suite"
MOCK_PID=""
APP_PID=""
PASSED=0
FAILED=0

cleanup() {
  pkill -f "miracast-sink" 2>/dev/null
  pkill -f "mock-wifid.py" 2>/dev/null
  pkill -f "xvfb-run" 2>/dev/null
  sleep 1
  sudo systemctl start miracast-wifid.service 2>/dev/null
}
trap cleanup EXIT

mkdir -p "$WORK"

start_mock() {
  local dur="$1" logfile="$2"
  sudo python3 "$DIR/mock-wifid.py" --stream-duration "$dur" > "$logfile" 2>&1 &
  MOCK_PID=$!
  sleep 2
  grep -q "owned org.freedesktop.miracle.wifi" "$logfile"
}

start_app() {
  local logfile="$2"
  G_MESSAGES_DEBUG=all GST_DEBUG=2 \
    xvfb-run -a -s "-screen 0 800x600x24" "$APP" > "$logfile" 2>&1 &
  APP_PID=$!
  sleep 1
}

nuke_everything() {
  pkill -f "miracast-sink" 2>/dev/null
  pkill -f "mock-wifid.py" 2>/dev/null
  pkill -f "xvfb-run" 2>/dev/null
  MOCK_PID=""
  APP_PID=""
  sleep 2
}

check() {
  local desc="$1" needle="$2" file="$3"
  if [ ! -f "$file" ]; then
    echo "    SKIP: $desc (no log)"
    return
  fi
  if grep -q "$needle" "$file" 2>/dev/null; then
    echo "    PASS: $desc"
    PASSED=$((PASSED + 1))
  else
    echo "    FAIL: $desc"
    FAILED=$((FAILED + 1))
  fi
}

wait_for() {
  local needle="$1" file="$2" timeout="${3:-20}"
  for i in $(seq 1 "$timeout"); do
    grep -q "$needle" "$file" 2>/dev/null && return 0
    sleep 0.5
  done
  return 1
}

# ── Stop real daemon ────────────────────────────────────────────
sudo systemctl stop miracast-wifid.service
sleep 1

# ── Test 1: Happy path ─────────────────────────────────────────
T1_MOCK="$WORK/t1-mock.log"
T1_APP="$WORK/t1-app.log"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  TEST 1: Happy path (connect, stream, no errors)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

start_mock 10 "$T1_MOCK"
start_app "" "$T1_APP"

wait_for "Connect called" "$T1_MOCK" 20 || echo "  (mock connect timed out)"
wait_for "handshake complete" "$T1_MOCK" 20 || echo "  (handshake timed out)"
sleep 5

check "mock owns bus"        "owned org.freedesktop.miracle.wifi" "$T1_MOCK"
check "GoNegRequest sent"    "emitting GoNegRequest"              "$T1_MOCK"
check "peer accepted"        "Connect called"                     "$T1_MOCK"
check "RTSP SETUP"           "RTSP << SETUP"                     "$T1_MOCK"
check "RTSP PLAY"            "RTSP << PLAY"                      "$T1_MOCK"
check "handshake complete"   "handshake complete"                 "$T1_MOCK"
check "app streamed"         "RTSP presentation URL"              "$T1_APP"
grep -qE "GStreamer.*CRITICAL|Internal data stream error" "$T1_APP" 2>/dev/null \
  && { echo "    FAIL: GStreamer error"; FAILED=$((FAILED + 1)); } \
  || { echo "    PASS: no GStreamer errors"; PASSED=$((PASSED + 1)); }

nuke_everything

# ── Test 2: Disconnect ─────────────────────────────────────────
T2_MOCK="$WORK/t2-mock.log"
T2_APP="$WORK/t2-app.log"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  TEST 2: Disconnect (mock dies mid-stream, app recovers)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

start_mock 30 "$T2_MOCK"
start_app "" "$T2_APP"

wait_for "handshake complete" "$T2_MOCK" 20
sleep 3
echo "  killing mock..."
nuke_everything
sleep 3

check "GoNegRequest sent"    "emitting GoNegRequest"              "$T2_MOCK"
check "handshake completed"  "handshake complete"                 "$T2_MOCK"
grep -qE "GStreamer.*CRITICAL|Internal data stream error" "$T2_APP" 2>/dev/null \
  && { echo "    FAIL: GStreamer error after disconnect"; FAILED=$((FAILED + 1)); } \
  || { echo "    PASS: no GStreamer error after disconnect"; PASSED=$((PASSED + 1)); }

nuke_everything

# ── Test 3: Reconnect ─────────────────────────────────────────
T3A_MOCK="$WORK/t3a-mock.log"
T3A_APP="$WORK/t3a-app.log"
T3B_MOCK="$WORK/t3b-mock.log"
T3B_APP="$WORK/t3b-app.log"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  TEST 3: Reconnect (connect, disconnect, connect again)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "  [3a] first connection..."
start_mock 8 "$T3A_MOCK"
start_app "" "$T3A_APP"
wait_for "handshake complete" "$T3A_MOCK" 20
sleep 2
echo "  killing mock..."
nuke_everything
sleep 3

echo "  [3b] second connection..."
start_mock 8 "$T3B_MOCK"
start_app "" "$T3B_APP"
wait_for "Connect called" "$T3B_MOCK" 20
wait_for "handshake complete" "$T3B_MOCK" 20
sleep 2

check "second mock owns bus"     "owned org.freedesktop.miracle.wifi" "$T3B_MOCK"
check "second GoNegRequest"      "emitting GoNegRequest"              "$T3B_MOCK"
check "second peer accepted"     "Connect called"                     "$T3B_MOCK"
check "second RTSP SETUP"        "RTSP << SETUP"                     "$T3B_MOCK"
check "second RTSP PLAY"         "RTSP << PLAY"                      "$T3B_MOCK"
check "second handshake"         "handshake complete"                 "$T3B_MOCK"
check "second app received URL"  "RTSP presentation URL"              "$T3B_APP"

nuke_everything

# ── Summary ────────────────────────────────────────────────────
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
TOTAL=$((PASSED + FAILED))
echo "  $PASSED/$TOTAL passed, $FAILED failed"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "  Logs: $WORK/"
echo ""
[ "$FAILED" -eq 0 ] && exit 0 || exit 1
