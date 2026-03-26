# Browser Compatibility Report

## Current Status

Status: first usable browser slices are working for local spectator mode and bridge-backed multiplayer connection

Target scope for this pass:

- browser build target
- canvas boot
- WebGL-backed rendering
- preloaded assets from Emscripten FS
- one usable local mode
- one bridge-backed browser client path to a native server

## Compatible or Promising Areas

- CMake build layout is straightforward to extend for Emscripten.
- SeriousProton already contains partial browser-specific code paths.
- A browser-specific main-loop path now exists in SeriousProton.
- Shader code is already GLSL ES style:
  - `attribute`
  - `varying`
  - `gl_FragColor`
  - `texture2D`
- Resource loading already goes through a virtualizable provider abstraction.
- Input is SDL-based, which maps well to Emscripten SDL2.
- A staged asset profile now allows a much smaller first-light preload for browser builds.

## Browser Risks and Gaps

### Main loop

- SeriousProton currently owns a native while-loop.
- Browser builds need `emscripten_set_main_loop_arg` instead.

### Networking

- Game networking is built around native TCP/UDP sockets.
- LAN discovery uses multicast UDP.
- Master-server scanning uses a background thread plus native address resolution.
- Browser-safe multiplayer now uses a websocket bridge seam.

### Persistence

- Preferences and keybindings currently save to a normal path.
- Browser builds now mount `/config` on IDBFS for persisted configuration and keybindings.

### Current rendering status

- The wasm target builds successfully and can be served locally.
- Headless browser checks now reach the shell `Running...` state with the minimal asset profile.
- Browser-specific startup breadcrumbs confirm the browser path reaches:
  - window creation
  - shader initialization
  - socketless local server creation
  - scenario startup
  - spectator screen construction
  - spectator first update
- A headless screenshot now shows the spectator radar scene rendered behind the shell overlay.
- The first-light rendering blocker is resolved for the local spectator slice.

### Current multiplayer status

- The browser build can now connect to a websocket bridge endpoint using a websocket-backed `GameClient` transport.
- A minimal local bridge prototype can proxy browser websocket traffic to a native EmptyEpsilon TCP server.
- Headless smoke testing has reached:
  - websocket upgrade
  - authentication exchange
  - client-id assignment
  - replicated multiplayer state
  - ship-selection UI with connected players visible
- Browser LAN/master-server browsing remains disabled.

### Optional subsystems

- Discord SDK: desktop only
- voice capture / network recorder: browser-policy and API mismatch
- hardware serial/DMX/Hue integrations: not browser-safe
- embedded HTTP server hosting: not browser-safe

## First Usable Browser Slice

Chosen first slice:

- local scenario
- spectator screen

Why:

- avoids browser multiplayer early
- still proves real rendering and input
- exercises real asset loading
- keeps native desktop path intact

## Follow-up Report Areas

- exact WebGL renderer limitations
- shader compatibility issues found at runtime
- browser-visible startup/render instrumentation cleanup
- websocket bridge hardening for native server interoperability
- asset trimming beyond the current minimal profile
- station-screen validation through the bridge path
