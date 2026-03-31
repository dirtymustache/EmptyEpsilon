#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build-wasm}"
SERIOUS_PROTON_DIR="${SERIOUS_PROTON_DIR:-$ROOT_DIR/../SeriousProton}"
EE_WASM_ASSET_PROFILE="${EE_WASM_ASSET_PROFILE:-minimal}"

if ! command -v emcmake >/dev/null 2>&1; then
    echo "error: emcmake was not found in PATH. Activate the Emscripten SDK first." >&2
    exit 1
fi

if [ ! -d "$SERIOUS_PROTON_DIR" ]; then
    echo "error: SeriousProton was not found at $SERIOUS_PROTON_DIR" >&2
    exit 1
fi

emcmake cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
    -DSERIOUS_PROTON_DIR="$SERIOUS_PROTON_DIR" \
    -DWITH_DISCORD=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DEE_WASM_ASSET_PROFILE="$EE_WASM_ASSET_PROFILE"

cmake --build "$BUILD_DIR" --target EmptyEpsilon
