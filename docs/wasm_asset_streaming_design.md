# Browser Asset Streaming Design

## Current State

The current browser build uses `EE_WASM_ASSET_PROFILE` to preload one Emscripten data payload at build time:

- `bridge` stages a narrow join-flow smoke-test asset set
- `minimal` stages all `scripts/` plus most of `resources/`
- `full` stages `scripts/`, `resources/`, and `packs/`

That keeps the browser target simple, but it still ties startup UX to a desktop-style staged asset tree. The browser shell in [`web/wasm_shell.html`](/C:/source/repos/epsilon/EmptyEpsilon/web/wasm_shell.html) boots the runtime directly, and the native HTTP helper in [`src/httpScriptAccess.cpp`](/C:/source/repos/epsilon/EmptyEpsilon/src/httpScriptAccess.cpp) currently only exposes admin/script endpoints rather than scenario asset metadata.

## Goals

- Reach the browser launcher and main menu with a small preload.
- Download only the assets needed for the selected scenario.
- Support both local browser-hosted sessions and bridge-connected native servers.
- Treat scenario changes as a manifest revision change, not as a hard-coded preload profile.
- Keep v1 explicit and easy to validate for new scenarios/assets.

## Manifest Shapes

### Bundle Definition

Checked-in under `asset_manifests/bundles/*.json`.

- `schema_version`
- `bundle_id`
- `description`
- `include[]`
  - `source`: repo-relative file or directory
  - `target`: runtime-relative mount path inside the browser FS

### Scenario Definition

Checked-in under `asset_manifests/scenarios/*.json`.

- `schema_version`
- `scenario_id`
- `scenario_file`
- `revision`
- `required_bundles[]`
- `optional_bundles[]`

### Generated Bundle Manifest

Produced in `build-wasm/asset_bundles/manifests/`.

- `schema_version`
- `bundle_id`
- `content_hash`
- `artifact_filename`
- `artifact_url`
- `fallback_url`
- `byte_size`

### Session Manifest

Published by the native HTTP server at runtime.

- `schema_version`
- `session_id`
- `scenario_file`
- `scenario_id`
- `scenario_revision`
- `manifest_revision`
- `scenario_manifest_url`
- `bundle_root`
- `fallback_bundle_root`
- `required_bundles[]`

## Runtime Flow

### Cold Boot

1. Browser loads the wasm/js/html shell plus the `core_browser_shell` preload.
2. Browser reaches launcher/menu/join UI without downloading scenario packs.

### Local Scenario Start

1. User starts a local browser session and reaches in-game scenario selection.
2. When a scenario is chosen, the browser requests the generated scenario manifest from the static web root.
3. Missing bundle artifacts are fetched and mounted into the runtime FS.
4. Once all required bundles are present, the local scenario starts.

### Remote Join

1. Browser shell sees `?bridge=...` and resolves the current runtime session manifest before entering gameplay.
2. It fetches any missing bundle artifacts using the static URL first and the native fallback URL second.
3. The join flow starts only after the required bundles are mounted.

### Scenario Change

1. The native server refreshes the published session manifest on each `startScenario(...)`.
2. Browser clients receive a lightweight revision change notification through replicated session fields.
3. The shell refetches the session manifest, downloads any missing bundles, shows a loading overlay, then resumes.

## Caching and Invalidation

- Bundle artifacts use content hashes in the filename and are safe for long-lived HTTP caching.
- Session manifests are versioned by a `manifest_revision` and should not be cached aggressively.
- The browser runtime also tracks loaded bundle hashes to avoid duplicate mounts in a single session.
- Cache correctness is hash-based, not scenario-name-based.

## V1 Scope

- Explicit checked-in manifests only.
- The initial implementation started with `scenario_00_basic.lua`.
- The current repo configuration uses one shared `scenario_resources_common` bundle for gameplay visuals and SFX, separate `.pack` bundles for ship/model content, and scenario-specific voice-over bundles where those assets exist.
- Bundle granularity, not per-file streaming.
- Static-host-first URLs with native-server fallback URLs embedded in manifests.

## Future Extension

- Add more checked-in scenario manifests over time.
- Split heavy station or audio payloads into optional bundles once the scenario path is stable.
- Add tooling to suggest manifest updates, but keep humans in control of the authoritative mapping.
