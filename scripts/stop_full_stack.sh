#!/usr/bin/env bash
# Stop all processes launched by start_full_stack.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STATE_FILE="$REPO_ROOT/logs/fullstack-state.json"

if [[ -f "$STATE_FILE" ]]; then
    for key in server_pid bridge_pid web_pid client_pid; do
        pid=$(python3 -c "import json,sys; d=json.load(open('$STATE_FILE')); v=d.get('$key'); print(v if v else '')" 2>/dev/null)
        if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
            echo "Stopping $key ($pid)..."
            kill "$pid" 2>/dev/null || true
        fi
    done
    rm -f "$STATE_FILE"
fi

# Belt-and-suspenders: clear any lingering listeners on the known ports
fuser -k 35666/tcp 35667/tcp 18086/tcp 2>/dev/null || true
# Also kill stale server on whatever admin port it used
if [[ -n "${ADMIN_PORT:-}" ]]; then
    fuser -k "${ADMIN_PORT}/tcp" 2>/dev/null || true
fi

sleep 0.5
echo "Done."
