# TODO

## Browser Scenario Asset Bundles

Explore moving the browser client away from one giant universal preload bundle and toward
server-selected scenario bundles.

Preferred direction:

1. Keep a small base browser bundle for shell/UI/bootstrap/common resources.
2. When the native server starts a new game, determine the asset bundle needed for that scenario.
3. Publish a scenario/session-specific bundle URL or manifest URL for browser clients.
4. Have the wasm client fetch the current scenario bundle before entering the game session.
5. Version bundle URLs by session ID, timestamp, or content hash so browser caches do not
   serve stale scenario data.

Why this is attractive:

- avoids forcing every browser client to download the full desktop-style asset set
- matches EmptyEpsilon's scenario-driven runtime model
- makes caching and invalidation easier than fully dynamic per-file streaming
- is likely simpler and more robust than trying to make the engine fetch arbitrary files on demand

Suggested first implementation slice:

- define a manifest format for `scenario_00_basic.lua`
- split assets into:
  - `base`
  - `scenario_00_basic`
  - optional station/spectator add-on bundles
- teach the browser launcher/bootstrap flow to fetch the scenario manifest and bundle URL
- keep manifests explicit at first rather than attempting automatic asset discovery

Open questions:

- where the authoritative scenario -> asset manifest mapping should live
- whether bundles should be prebuilt or assembled when the server starts a new scenario
- how the browser client learns the current bundle URL:
  - websocket handshake metadata
  - HTTP endpoint
  - session manifest file
- whether this should later evolve into a more general lazy-loading asset system

## Shared Wasm Core / Host Shell Architecture

Explore making the WebAssembly build the primary EmptyEpsilon app core, with
multiple host environments providing platform capabilities around that core.

Preferred direction:

1. Treat the game/runtime logic as a shared wasm core instead of treating the
   browser build as a one-off port.
2. Support separate host implementations over that core:
   - `browser` host for browser-safe capabilities
   - `native shell` host for OS-backed capabilities
3. Keep gameplay logic inside the wasm core and move platform-specific behavior
   behind explicit host capability boundaries.
4. Prioritize a desktop shell host as the first non-browser target.
5. Keep mobile shell hosts as follow-on work, with iOS constrained to bundled,
   self-contained wasm content.

Host capability model to define first:

- networking
- filesystem and persistence
- windowing and input
- audio input and output
- process/admin integration

Important direction:

- browsers should continue using browser-safe networking such as websocket-based
  transport boundaries
- shell hosts should be allowed to provide richer capabilities such as raw
  sockets, fuller filesystem access, and native process/window integration
- temporary bridge-specific seams should eventually be replaced by host-provided
  capability adapters
- small host-specific bootstrap glue is acceptable even if the core runtime is shared

Current host/runtime landscape notes:

- there are hosted/runtime environments that expose capabilities to wasm apps,
  but there is not one universal shell API that can replace all platform glue
- Cloudflare Workers, Fermyon Spin, and wasmCloud are relevant examples of
  capability-hosted wasm environments
- Wasmtime + WASI is a strong candidate runtime for building a custom native
  shell host
- these environments are best treated as host-specific adapters, not as a
  single portable target for the whole game
- the practical architecture should still assume:
  - one shared wasm core
  - one browser host
  - one custom desktop shell host
  - optional future adapters for other wasm hosts if they become useful

Suggested first implementation slice:

1. Document the host capability interfaces before changing core runtime code.
2. Formalize networking as the first host boundary.
3. Implement one desktop native shell host path first.
4. Keep the browser path working as-is while moving platform assumptions behind
   the host layer over time.
5. Defer mobile-shell details until the desktop-shell shape is proven.
