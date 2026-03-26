# EmptyEpsilon Wasm Port Plan

## Readiness Snapshot

EmptyEpsilon is a good candidate for a staged browser port:

- The build is CMake-based and already separates the game from the SeriousProton engine.
- Rendering is mostly shader-driven and already targets a GLES-style API surface.
- Assets are loaded through SeriousProton resource providers, which maps well to Emscripten's virtual filesystem.
- There is already partial Emscripten awareness inside SeriousProton for GL loading, HTTP requests, and browser-gated audio startup.

The main blockers are at the platform boundary:

- SeriousProton still uses a desktop-style blocking main loop instead of Emscripten's callback-driven loop.
- Multiplayer uses native TCP/UDP sockets and LAN/master-server discovery that are not browser safe.
- Config and user data assume desktop filesystem paths.
- Some optional subsystems are desktop-only: Discord, hardware output devices, voice capture, and HTTP server hosting.
- The project depends on a sibling `../SeriousProton` checkout, so browser support needs coordinated changes in both trees.

## Recon Findings

### Build system

- Primary build system: `CMakeLists.txt`
- Main executable target: `EmptyEpsilon`
- Engine dependency: `add_subdirectory("${SERIOUS_PROTON_DIR}" ...)`
- Extra dependency fetched at configure time: `meshoptimizer`

### App startup

- Game entry point: `src/main.cpp`
- Window/render chain setup: `src/init/displaywindows.cpp`
- Config path and option loading: `src/init/config.cpp`
- Resource path registration: `src/init/resources.cpp`

### Rendering

- Window + GL context creation live in `../SeriousProton/src/windowManager.cpp`
- GL loader and ES detection live in `../SeriousProton/src/graphics/opengl.cpp`
- Core shader registry: `src/shaderRegistry.cpp`
- 3D viewport path: `src/screenComponents/viewport3d.cpp`
- Particle rendering: `src/particleEffect.cpp`
- Shaders live in `resources/shaders/*.shader`

### Assets and filesystem

- Resources are discovered through `DirectoryResourceProvider` and pack files.
- EmptyEpsilon registers `resources/`, `scripts/`, and `packs/`.
- Preferences and keybindings are read from a user config directory.
- Browser packaging can preload `resources`, `scripts`, and `packs` directly into `/resources`, `/scripts`, and `/packs`.

### Audio

- Sound manager: `../SeriousProton/src/soundManager.cpp`
- SDL audio backend: `../SeriousProton/src/audio/source.cpp`
- Voice capture / network audio recorder exists and is not suitable for the first browser slice.

### Networking

- Game client: `../SeriousProton/src/multiplayer_client.cpp`
- Game server: `../SeriousProton/src/multiplayer_server.cpp`
- LAN/master server discovery: `../SeriousProton/src/multiplayer_server_scanner.cpp`
- EmptyEpsilon browser/join UI currently assumes native sockets and discovery flows.
- SeriousProton has an HTTP websocket helper, but gameplay networking is still bound to native `StreamSocket` TCP/UDP paths.

### Threads and async assumptions

- SeriousProton server scanner uses `std::thread`.
- EmptyEpsilon hardware device integrations also use threads.
- Browser target should avoid invoking these paths in the first milestone.

## Attack Plan

### Phase 1: Browser build scaffold

- Add an Emscripten-aware executable target in EmptyEpsilon CMake.
- Add a browser shell HTML and a convenience `scripts/build_wasm.sh`.
- Stage browser assets before link so the first milestone can use a lean preload profile.
- Default browser builds to a `minimal` asset set instead of preloading every desktop asset pack.

### Phase 2: Browser entry path

- Add a browser bootstrap path that starts a local scenario and opens `SpectatorScreen`.
- Skip desktop-only runtime features in the browser path:
  - Discord
  - voice capture
  - external hardware configuration

### Phase 3: Main loop compatibility

- Patch SeriousProton to expose a browser-safe frame pump and use `emscripten_set_main_loop_arg`.
- Keep native desktop loop unchanged.

### Phase 4: Rendering triage

- Start with the existing shader pipeline and current GL ES-friendly code path.
- Verify canvas boot, frame rendering, and mouse/keyboard input.
- Log remaining WebGL-specific gaps in `docs/wasm_renderer_notes.md`.

### Phase 5: Assets and persistence

- Use Emscripten preload for the initial slice.
- Keep config/keybindings on browser-backed IDBFS storage.
- Continue trimming preload profiles so browser smoke tests do not need the full desktop asset set.

### Phase 6: Networking seam

- Do not force raw native TCP/UDP sockets into the browser target.
- Route browser multiplayer through a websocket-backed client transport.
- Keep LAN discovery disabled in-browser and use a websocket bridge for native server compatibility.

## First Milestone Definition

The first browser milestone is:

- buildable with Emscripten from documented commands
- launches in a browser and shows a canvas
- loads packaged assets from the wasm filesystem
- enters one usable mode automatically
- uses a local scenario plus spectator view instead of requiring browser multiplayer on day one

This keeps scope narrow while proving the major platform seams: startup, rendering, assets, input, and browser main loop.

## Current Milestone Progress

- Browser builds now complete successfully with Emscripten.
- The `minimal` wasm asset profile reduces the browser data bundle from roughly 332 MB to roughly 75 MB for first-light testing.
- The `bridge` wasm asset profile reduces menu/connect smoke-test data bundles to roughly 2.7 MB.
- Local HTTP serving and browser launch are reproducible.
- The browser path now reaches a working local spectator slice with real scene rendering in headless verification.
- Browser bootstrap uses a socketless local server path so the first milestone does not depend on native listeners.
- Browser config and keybindings now persist through an IDBFS-backed `/config` mount.
- Browser multiplayer now has a working websocket bridge seam:
  - browser wasm client
  - websocket bridge
  - native EmptyEpsilon server
- Smoke testing has reached the multiplayer ship-selection UI against a native server, including replicated connected-player state.
- The next blockers are:
  - making the bridge flow less diagnostics-oriented in the browser UX
  - validating one full station-screen path through the bridge
  - deciding whether to add reconnect behavior and a more polished bridge launcher
