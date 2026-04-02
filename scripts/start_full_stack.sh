#!/usr/bin/env bash
# Start the EmptyEpsilon full stack: native server, WS bridge, wasm HTTP server, native client.
# Usage: ./scripts/start_full_stack.sh [options]
#   --scenario SCENARIO     (default: scenario_00_basic.lua)
#   --native-username NAME  (default: native_user)
#   --web-username NAME     (default: web_user)
#   --station STATION       station for web client (default: relay)
#   --native-station STATION station for native client (default: helms)
#   --bind-host HOST        (default: 127.0.0.1)
#   --server-port PORT      (default: 35666)
#   --bridge-port PORT      (default: 35667)
#   --web-port PORT         (default: 18086)
#   --admin-port PORT       (default: 8181)
#   --no-native-client      skip launching native client
#   --no-browser            skip opening browser tabs
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build"
BUILD_WASM_DIR="$REPO_ROOT/build-wasm"
LOG_DIR="$REPO_ROOT/logs/fullstack-$(date +%Y%m%d-%H%M%S)"
STATE_FILE="$REPO_ROOT/logs/fullstack-state.json"

SCENARIO="scenario_00_basic.lua"
NATIVE_USERNAME="native_user"
WEB_USERNAME="web_user"
STATION="relay"
NATIVE_STATION="helms"
BIND_HOST="127.0.0.1"
SERVER_PORT=35666
BRIDGE_PORT=35667
WEB_PORT=18086
ADMIN_PORT=8181
NO_NATIVE_CLIENT=0
NO_BROWSER=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --scenario)         SCENARIO="$2";         shift 2 ;;
        --native-username)  NATIVE_USERNAME="$2";  shift 2 ;;
        --web-username)     WEB_USERNAME="$2";     shift 2 ;;
        --station)          STATION="$2";          shift 2 ;;
        --native-station)   NATIVE_STATION="$2";   shift 2 ;;
        --bind-host)        BIND_HOST="$2";        shift 2 ;;
        --server-port)      SERVER_PORT="$2";      shift 2 ;;
        --bridge-port)      BRIDGE_PORT="$2";      shift 2 ;;
        --web-port)         WEB_PORT="$2";         shift 2 ;;
        --admin-port)       ADMIN_PORT="$2";       shift 2 ;;
        --no-native-client) NO_NATIVE_CLIENT=1;    shift   ;;
        --no-browser)       NO_BROWSER=1;          shift   ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

NATIVE_EXE="$BUILD_DIR/EmptyEpsilon"
WASM_HTML="$BUILD_WASM_DIR/EmptyEpsilon.html"

if [[ ! -x "$NATIVE_EXE" ]]; then
    echo "error: native binary not found at $NATIVE_EXE" >&2
    exit 1
fi
if [[ ! -f "$WASM_HTML" ]]; then
    echo "error: wasm build not found at $WASM_HTML" >&2
    exit 1
fi

# Kill any previous full-stack processes
STOP_SCRIPT="$REPO_ROOT/scripts/stop_full_stack.sh"
if [[ -f "$STOP_SCRIPT" ]]; then
    bash "$STOP_SCRIPT" 2>/dev/null || true
fi

mkdir -p "$LOG_DIR" "$REPO_ROOT/logs"

WEB_URL="http://${BIND_HOST}:${WEB_PORT}/EmptyEpsilon.html?bridge=ws://${BIND_HOST}:${BRIDGE_PORT}&station=${STATION}&username=${WEB_USERNAME}"
ADMIN_URL="http://${BIND_HOST}:${WEB_PORT}/admin.html"

echo "Starting native server..."
(
    cd "$REPO_ROOT"
    "$NATIVE_EXE" \
        headless=1 \
        server_scenario="$SCENARIO" \
        server_port="$SERVER_PORT" \
        httpserver="$ADMIN_PORT"
) >"$LOG_DIR/server.out.log" 2>"$LOG_DIR/server.err.log" &
SERVER_PID=$!

echo "Starting WebSocket bridge..."
python3 "$REPO_ROOT/scripts/wasm_ws_bridge.py" \
    --listen-host "$BIND_HOST" \
    --listen-port "$BRIDGE_PORT" \
    --target-host 127.0.0.1 \
    --target-port "$SERVER_PORT" \
    --verbose \
    >"$LOG_DIR/bridge.out.log" 2>"$LOG_DIR/bridge.err.log" &
BRIDGE_PID=$!

echo "Starting wasm HTTP server..."
python3 "$REPO_ROOT/scripts/serve_wasm.py" \
    --host "$BIND_HOST" \
    --port "$WEB_PORT" \
    --directory "$BUILD_WASM_DIR" \
    --extra-static-root "$REPO_ROOT/web" \
    --proxy-admin-base "http://127.0.0.1:${ADMIN_PORT}" \
    >"$LOG_DIR/serve.out.log" 2>"$LOG_DIR/serve.err.log" &
WEB_PID=$!

sleep 2

CLIENT_PID=""
if [[ "$NO_NATIVE_CLIENT" -eq 0 ]]; then
    echo "Starting native client..."
    (
        cd "$REPO_ROOT"
        "$NATIVE_EXE" \
            "autoconnect=$NATIVE_STATION" \
            "autoconnect_address=127.0.0.1:${SERVER_PORT}" \
            autoconnectship=solo \
            "username=$NATIVE_USERNAME"
    ) >"$LOG_DIR/client.out.log" 2>"$LOG_DIR/client.err.log" &
    CLIENT_PID=$!
fi

# Write state file for stop script
cat > "$STATE_FILE" <<EOF
{
  "server_pid": $SERVER_PID,
  "bridge_pid": $BRIDGE_PID,
  "web_pid": $WEB_PID,
  "client_pid": ${CLIENT_PID:-null},
  "web_url": "$WEB_URL",
  "admin_url": "$ADMIN_URL",
  "bind_host": "$BIND_HOST",
  "server_port": $SERVER_PORT,
  "bridge_port": $BRIDGE_PORT,
  "web_port": $WEB_PORT,
  "admin_port": $ADMIN_PORT,
  "log_dir": "$LOG_DIR"
}
EOF

echo ""
echo "Full stack started."
echo "  Server PID : $SERVER_PID  (log: $LOG_DIR/server.out.log)"
echo "  Bridge PID : $BRIDGE_PID  (log: $LOG_DIR/bridge.out.log)"
echo "  Web PID    : $WEB_PID     (log: $LOG_DIR/serve.out.log)"
[[ -n "$CLIENT_PID" ]] && echo "  Client PID : $CLIENT_PID  (log: $LOG_DIR/client.out.log)"
echo ""
echo "  Web client : $WEB_URL"
echo "  Admin      : $ADMIN_URL"
echo ""

if [[ "$NO_BROWSER" -eq 0 ]]; then
    if command -v xdg-open >/dev/null 2>&1; then
        xdg-open "$WEB_URL" 2>/dev/null &
        xdg-open "$ADMIN_URL" 2>/dev/null &
    fi
fi
