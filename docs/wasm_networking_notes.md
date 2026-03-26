# Wasm Networking Notes

## Current State

The browser target now supports two networking-adjacent slices:

- local-only spectator bootstrap with no native listeners
- browser client connection through a websocket bridge to a native EmptyEpsilon server

The browser target still does not support:

- direct TCP client connections from wasm
- UDP LAN discovery
- multicast discovery
- native master-server browse flows

## Why

The desktop EmptyEpsilon and SeriousProton networking stack assumes native sockets:

- `GameClient` uses a stream socket transport designed for native TCP
- `GameServer` listens on native TCP and advertises over UDP
- `ServerScanner` uses multicast/broadcast plus HTTP master-server queries

Browsers do not expose that socket model directly, so browser multiplayer needs a browser-safe transport boundary.

## Implemented Browser Transport

For `__EMSCRIPTEN__` builds:

- browser menu flow accepts a manual websocket bridge URL
- the wasm shell can launch directly into the bridge flow with `?bridge=ws://...`
- the wasm shell can also request a preferred station with `?station=relay` for first-usable browser client slices
- browser `GameClient` can use a websocket-backed `StreamSocket` implementation
- native LAN/master-server browser flows remain disabled in browser builds

The lean `bridge` asset profile now includes the resources needed for bridge connection plus early station-screen validation, not just the join menu.

This keeps the existing replication and gameplay packet handling intact while moving only the transport boundary.

## Implemented Bridge Prototype

A minimal local bridge prototype now exists in:

- `scripts/wasm_ws_bridge.py`

Current behavior:

- accepts a websocket connection from the browser
- opens a native TCP connection to an EmptyEpsilon server
- forwards browser websocket frames to TCP
- repacketizes native TCP data into discrete websocket frames using the EmptyEpsilon packet length prefix

That packetization step is important because the browser websocket transport is message-oriented while the native server side is stream-oriented.

## Verified Flow

The current smoke-tested path is:

1. browser wasm client
2. websocket bridge
3. native EmptyEpsilon server

This has been verified far enough to reach:

- websocket upgrade
- authentication request/response
- `CMD_SET_CLIENT_ID`
- replicated connected-player state
- ship-selection UI in the browser

The browser flow now also has a narrow auto-join path that can:

- claim the first available player ship
- request a preferred station or main screen
- launch directly into that UI without manual ship-selection clicks

That preferred-station choice is now available from both:

- the wasm HTML shell
- the in-game browser websocket-join menu

The browser path also now emits clearer status/diagnostic breadcrumbs for:

- websocket bridge connection
- waiting for replicated player state
- waiting for a player ship to appear
- waiting for claimed ship assignment
- preferred-station launch or fallback

## Remaining Gaps

- no browser-side LAN or internet browse list
- bridge service is a development prototype, not production-hardened
- reconnect/error UX is still fairly diagnostic-oriented
- station-screen validation through the bridge still needs focused testing

## Recommended Next Steps

1. Validate one full station path through the websocket bridge, not just ship selection.
2. Improve browser UX around connect, disconnect, and reconnect.
3. Decide whether the bridge stays as a standalone helper or becomes a supported companion service.
