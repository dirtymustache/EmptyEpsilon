# Wasm Branch Notes

This browser milestone currently spans two local repositories:

- EmptyEpsilon repo: branch `wasm-browser-slice`
- SeriousProton repo: branch `wasm-browser-slice`

Local commit pointers at the time this note was written:

- EmptyEpsilon: `498b352d` - `Add first-pass wasm browser spectator target`
- SeriousProton: `8db11b3` - `Add browser-oriented wasm runtime support`

The EmptyEpsilon browser target depends on the matching SeriousProton branch/commit above.

Current milestone state:

- Emscripten browser target builds locally
- minimal asset profile is enabled for first-light testing
- browser bootstrap runs a socketless local scenario path
- spectator mode renders in browser verification

Still intentionally deferred:

- IDBFS-backed persistence
- browser-compatible multiplayer transport
- websocket bridge to native EmptyEpsilon servers
