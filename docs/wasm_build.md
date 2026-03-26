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

For a smaller bridge/station smoke-test build, use:

```bash
EE_WASM_ASSET_PROFILE=bridge ./scripts/build_wasm.sh
```

This stages the browser bridge path and first station-screen assets:

- `resources/gui/`
- `resources/locale/`
- `resources/cursors/`
- `resources/radar/`
- `resources/shaders/`
- `resources/sfx/`
- a few logo images
- shared radar/station textures such as `waypoint.png`, `redicule.png`, and `noise.png`
- no `scripts/`
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

To launch the browser client straight into the websocket bridge path instead of the
local spectator bootstrap, append a `bridge` query parameter:

```text
http://localhost:8000/EmptyEpsilon.html?bridge=ws://127.0.0.1:35667
```

Optional query parameters:

- `bridge=ws://host:port` or `wss://host:port` to open the browser bridge join flow
- `station=relay|science|helms|weapons|engineering|operations|tactical|singlepilot|mainscreen` to request a browser auto-join station after bridge connection
- `mode=menu` to skip local spectator bootstrap and open the in-game main menu
- `bootstrap=0|1` to disable or force the local spectator bootstrap
- `scenario=scenario_00_basic.lua` to override the browser bootstrap scenario
- `username=web_user` to seed the player name in browser sessions

The HTML shell now also exposes launch controls for:

- local spectator
- main menu
- bridge connect

So the browser target can be switched between the main slices without manually editing the URL.
The in-game browser join menu also exposes the same preferred-station choice before connecting to the websocket bridge.

## Optional WebSocket Bridge Prototype

For browser-to-native multiplayer experiments, a minimal local bridge script is now included:

```bash
python scripts/wasm_ws_bridge.py --listen-port 35667 --target-host 127.0.0.1 --target-port 35666
```

That gives the browser client a local websocket endpoint such as:

```text
ws://127.0.0.1:35667
```

This bridge is only a local prototype. It forwards websocket binary messages to a native
EmptyEpsilon TCP server and is not hardened for production use.

## Current Browser Behavior

- The browser build boots directly into a local scenario spectator slice.
- The wasm shell can optionally open the websocket bridge client path directly with `?bridge=...`.
- The wasm shell can request a preferred bridge station with `?station=relay` and auto-claim the first available player ship.
- The wasm shell can also launch into the main menu with `?mode=menu`.
- The `minimal` profile currently produces a much smaller `.data` bundle than the original full preload.
- The `bridge` profile is intended for multiplayer smoke tests and first station-screen validation, while keeping the preload much smaller than the spectator-oriented `minimal` profile.
- Headless browser verification now reaches real spectator rendering, not just the shell canvas.
- Browser multiplayer can now reach the ship-selection flow of a native server through the websocket bridge prototype.
- Browser multiplayer still depends on a websocket bridge and is not LAN/master-server compatible yet.
- Config and keybindings now persist through IDBFS-backed `/config` storage.
- Browser startup uses a socketless local server path for the first milestone instead of native TCP/UDP listeners.
