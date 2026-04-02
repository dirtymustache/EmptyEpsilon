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
