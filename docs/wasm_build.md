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

This now stages the full runtime asset set by default:

- all `resources/`
- all `scripts/`
- all `packs/`

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
  -DCMAKE_BUILD_TYPE=RelWithDebInfo

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

- `bridge=ws://host:port`, `wss://host:port`, or a same-origin proxied path such as `wss://example.com/bridge/` to open the browser bridge join flow
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

This bridge is still intentionally minimal. It forwards websocket binary messages to a native
EmptyEpsilon TCP server. If you want to put it behind a reverse proxy later, a same-origin path
such as `/bridge/` works well for browser clients.

When using a hosted reverse-proxied bridge, prefer a URL like `wss://example.com/bridge/`
instead of exposing a separate `:35667` port publicly. The browser/runtime code now treats
that as the production default on standard `https` origins, while local nonstandard ports
still default to `ws://host:35667` for development.

## Optional HTTP Server Control

The headless EmptyEpsilon server also has an experimental HTTP Lua endpoint. To enable it,
start the native server with an `httpserver` preference:

```bash
./build-win-msvc/EmptyEpsilon.exe headless=scenario_00_basic.lua server_port=35666 httpserver=8080
```

That enables:

- `POST /exec.lua` to run Lua code against the current game

A helper script is included for the common cases:

```powershell
./scripts/ee_server_control.ps1 -Action new-game -Scenario scenario_00_basic.lua
./scripts/ee_server_control.ps1 -Action pause
./scripts/ee_server_control.ps1 -Action unpause
./scripts/ee_server_control.ps1 -Action lua -Code 'setScenario("scenario_03_waves.lua")'
```

By default the helper targets `http://127.0.0.1:8080/exec.lua`, but `-ServerHost` and `-Port`
can be overridden if needed.

## Current Browser Behavior

- The browser build preloads the full current runtime asset set, including `packs/`.
- The wasm shell can optionally open the websocket bridge client path directly with `?bridge=...`.
- The wasm shell can request a preferred bridge station with `?station=relay` and auto-claim the first available player ship.
- The wasm shell can also launch into the main menu with `?mode=menu`.
- Browser multiplayer can now reach the ship-selection flow of a native server through the websocket bridge prototype.
- Browser multiplayer still depends on a websocket bridge and is not LAN/master-server compatible yet.
- Config and keybindings now persist through IDBFS-backed `/config` storage.
