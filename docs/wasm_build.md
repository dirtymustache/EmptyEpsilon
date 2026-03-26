# Building the Browser Target

## Prerequisites

- Emscripten SDK installed and activated
- CMake
- Ninja
- A sibling SeriousProton checkout at `../SeriousProton`

## Quick Build

From the EmptyEpsilon repo root:

```bash
./scripts/build_wasm.sh
```

This defaults to the `minimal` browser asset profile, which stages:

- `resources/` without the heavyweight `audio/` and `music/` folders
- all `scripts/`
- no `packs/`

To build the heavier preload set instead:

```bash
EE_WASM_ASSET_PROFILE=full ./scripts/build_wasm.sh
```

This configures an Emscripten build in `build-wasm/` and produces:

- `build-wasm/EmptyEpsilon.html`
- `build-wasm/EmptyEpsilon.js`
- `build-wasm/EmptyEpsilon.wasm`
- `build-wasm/EmptyEpsilon.data`
- staged preload content under `build-wasm/wasm_assets/`

## Manual Configure

```bash
emcmake cmake -S . -B build-wasm -G Ninja \
  -DSERIOUS_PROTON_DIR=../SeriousProton \
  -DWITH_DISCORD=OFF \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DEE_WASM_ASSET_PROFILE=minimal

cmake --build build-wasm --target EmptyEpsilon
```

The browser-specific link flags and preload mounting are now handled in `CMakeLists.txt`.

## Local Serve

Emscripten output must be served over HTTP. One simple option:

```bash
python -m http.server 8000 --directory build-wasm
```

Then open:

```text
http://localhost:8000/EmptyEpsilon.html
```

## Current Browser Behavior

- The browser build boots directly into a local scenario spectator slice.
- The `minimal` profile currently produces a much smaller `.data` bundle than the original full preload.
- Headless browser verification now reaches real spectator rendering, not just the shell canvas.
- Multiplayer connection flow is not enabled for browser use yet.
- Config and keybindings are session-local for now; persistent IDBFS sync is still TODO.
- Browser startup uses a socketless local server path for the first milestone instead of native TCP/UDP listeners.
