![EmptyEpsilon logo](https://raw.githubusercontent.com/daid/EmptyEpsilon/master/resources/logo_full.png)

Started as a cross-platform, open-source "clone" of [Artemis Spaceship Bridge Simulator](https://www.artemisspaceshipbridge.com/), **EmptyEpsilon** has already deviated from Artemis with new features and gameplay, including a Game Master mode and multiple AI factions. We strive to get EmptyEpsilon working on several platforms, and Windows, Linux, and Android are fully supported.

The game is written in C++ with the [SeriousProton](https://github.com/daid/SeriousProton) engine and uses [SDL2](http://www.libsdl.org/) for most of the heavy lifting.

## Download and install

Official releases for Windows, Linux (as a .deb package), and Android (beta quality) are available from the [EmptyEpsilon website](https://daid.github.io/EmptyEpsilon/#tabs=5) or [GitHub releases](https://github.com/daid/EmptyEpsilon/releases). Make sure the host and all players run the same version number of EmptyEpsilon; otherwise, players won't be able to connect.

-   Windows releases are distributed as self-contained ZIP archives that don't need installation. You can expand the ZIP archive and launch EmptyEpsilon directly from the expanded folder.
-   The .deb package requires freetype and SDL2 packages to be installed on your Linux distribution.
-   The Android APK is built for the `armeabi-v7a` ABI and should launch on most Android phones and tablets with ARM processors. (For ARM v8, see [the wiki](https://github.com/daid/EmptyEpsilon/wiki/Build%5CAndroid#build-for-64-bit-arm-v8).) The official ARM APK won't install on Intel x86 or x86_64 devices running Android, but is compatible with [Android Emulator system images that support ARM ABIs](https://android-developers.googleblog.com/2020/03/run-arm-apps-on-android-emulator.html). To build a 32- or 64-bit x86 APK, see [the wiki](https://github.com/daid/EmptyEpsilon/wiki/Build%5CAndroid#build-for-x86).

### Configuration files

EmptyEpsilon settings are stored in an `options.ini` file located in either the `.emptyepsilon` directory of your user home or the same directory as the EmptyEpsilon launcher. For details, see [this repository's wiki](https://github.com/daid/EmptyEpsilon/wiki/Preferences-file).

### Build from source

See this repository's wiki for guidance on [building EmptyEpsilon from source](https://github.com/daid/EmptyEpsilon/wiki/Build). Several Build subpages on the wiki provide steps for building on specific operating systems, distributions, or hardware.

### WebAssembly browser build

An experimental browser target is available for running EmptyEpsilon as a WebAssembly + WebGL app with Emscripten.

Current scope:

-   boots in a browser canvas
-   supports a local spectator slice
-   supports a browser client path through a local WebSocket bridge to a native EmptyEpsilon server
-   includes an admin page for HTTP-based server control and status

Current limitations:

-   still experimental
-   browser multiplayer requires the bridge helper and a native server
-   direct LAN discovery and native socket networking are not available in the browser
-   host and browser client still need to run the same EmptyEpsilon version

#### Prerequisites

-   Emscripten SDK installed and activated
-   CMake
-   Ninja
-   a sibling [SeriousProton](https://github.com/daid/SeriousProton) checkout at `../SeriousProton`

#### Build the browser target

From the EmptyEpsilon repository root:

```bash
./scripts/build_wasm.sh
```

To build the heavier browser preload that includes the full staged asset set:

```bash
EE_WASM_ASSET_PROFILE=full ./scripts/build_wasm.sh
```

This produces browser artifacts in `build-wasm/`, including:

-   `build-wasm/EmptyEpsilon.html`
-   `build-wasm/EmptyEpsilon.js`
-   `build-wasm/EmptyEpsilon.wasm`
-   `build-wasm/EmptyEpsilon.data`

#### Serve the browser build

Use the local cache-aware helper instead of a plain static file server:

```bash
python scripts/serve_wasm.py --host 127.0.0.1 --port 18086 --directory build-wasm
```

Then open:

```text
http://127.0.0.1:18086/EmptyEpsilon.html
```

#### Run the full local browser stack

1. Start a native server:

```powershell
.\build-win-msvc\EmptyEpsilon.exe headless=scenario_00_basic.lua server_port=35666 httpserver=8080
```

2. Start the WebSocket bridge:

```bash
python scripts/wasm_ws_bridge.py --listen-host 127.0.0.1 --listen-port 35667 --target-host 127.0.0.1 --target-port 35666 --verbose
```

3. Start the browser HTTP server:

```bash
python scripts/serve_wasm.py --host 127.0.0.1 --port 18086 --directory build-wasm
```

4. Open the browser client:

```text
http://127.0.0.1:18086/EmptyEpsilon.html?bridge=ws://127.0.0.1:35667&station=relay&username=web_user
```

#### Useful browser pages

-   Main browser launcher:
    `http://127.0.0.1:18086/EmptyEpsilon.html`
-   Browser bridge launch example:
    `http://127.0.0.1:18086/EmptyEpsilon.html?bridge=ws://127.0.0.1:35667&station=relay&username=web_user`
-   Server admin page:
    `http://127.0.0.1:18086/admin.html`

The admin page talks to the experimental HTTP Lua endpoint on the native server and shows current server status, scenario, mission time, pause state, and connected players.

More browser-target details are documented in:

-   [docs/wasm_build.md](docs/wasm_build.md)
-   [docs/wasm_networking_notes.md](docs/wasm_networking_notes.md)
-   [docs/wasm_compatibility_report.md](docs/wasm_compatibility_report.md)

## Community

For information on EmptyEpsilon's Discord and forums communities, and regularly planned hosted game sessions, see the [EmptyEpsilon website](https://daid.github.io/EmptyEpsilon/#tabs=6). If you run public EmptyEpsilon games or use it in your gaming projects, [file an issue](https://github.com/daid/EmptyEpsilon/issues) to request to be added to that page.

## Contribute

If you want to contribute, we're mostly looking for awesome models, sound effects, and music. The game is tested regulary by some of our trusty colleagues.

Some general contribution rules:

1.  This project is a dictatorship. Yes, it's open source, but we'd much rather spend time on building what we like than arguing with people.

2.  Be precise when filing issues. Explain why you posted the issue, what you expect, what is happening, why is your feature worth the time to develop it, what operating system is affected, etc. Unclear issues are subject to rule 1 with extreme prejudice.

3.  Despite the above two, we very much value input, feedback, and suggestions from people playing EmptyEpsilon. If you have ideas or want to donate beer, drop us a line.

### Donate

If you don't have the skills to help code or create models but want to give something back, you can always donate a bit. All donations go directly toward buying better assets for the game (in this case, more and better 3D models). You can find the instructions on the [EmptyEpsilon website](http://daid.github.io/EmptyEpsilon/).

### Write code

If you are a coder and want to contribute, there are a few things to take into account.

1.  The code is a undocumented mess at this point. We're working on fixing that.

2.  We use the following conventions:

    -   Member values use underscores to separate words (`zoom_level`).
    -   Classes use HighCamelCase (`GuiSlider`).
    -   Functions use lowCamelCase (`getZoomLevel`).

3.  Use a single pull request to change a single thing. Want to change multiple things? File multiple requests.

### Provide art

There is no clear goal where this game is going. This means that there is no formal game, art, or asset design. If you have something that you would like to see in this game (or want to make something), drop us a line. We'd love to see what you can do and how you can help improve the game.

For details on how EmptyEpsilon uses 3D models, see [this repository's wiki](https://github.com/daid/EmptyEpsilon/wiki/Adding-3D-models).

### Translate and localize

For a guide to translating EmptyEpsilon and its scenarios, see [this repository's wiki](https://github.com/daid/EmptyEpsilon/wiki/Translation-and-Localization).

## Documentation

Basic documentation for setting up and running games is available on the [EmptyEpsilon website](https://daid.github.io/EmptyEpsilon/#tabs=2).

To learn EmptyEpsilon gameplay fundamentals, read the [website's stations profiles](https://daid.github.io/EmptyEpsilon/#tabs=3) and play through the game's built-in tutorial mode available from the main menu, which covers each crew member's interface and responsibilities.

For guidance in scripting scenarios, see the [website's mission scripting guide](https://daid.github.io/EmptyEpsilon/#tabs=4). For a scripting API reference, open the `script_reference.html` file included in your version's downloaded archive, which is specific to that version of EmptyEpsilon.

For documentation on the game's preferences file and command-line options, hardware and DMX support, more complex internet play configurations like headless and proxy servers, enabling and using EmptyEpsilon's HTTP API server, or adding ship templates and models, see [this repository's wiki](https://github.com/daid/EmptyEpsilon/wiki).
