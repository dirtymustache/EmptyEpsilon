#include <memory>
#include <set>
#include <filesystem>
#include <cstdlib>
#include <string.h>
#include <i18n.h>
#include <multiplayer_proxy.h>
#ifdef _MSC_VER
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <sys/types.h>
#include "textureManager.h"
#include "soundManager.h"
#include "gui/theme.h"
#include "menus/mainMenus.h"
#include "menus/autoConnectScreen.h"
#include "menus/joinServerMenu.h"
#include "menus/shipSelectionScreen.h"
#include "multiplayer_client.h"
#include "screens/spectatorScreen.h"
#include "main.h"
#include "epsilonServer.h"
#include "httpScriptAccess.h"
#include "preferenceManager.h"
#include "networkRecorder.h"
#include "tutorialGame.h"
#include "windowManager.h"
#include "init/config.h"
#include "init/resources.h"
#include "init/displaywindows.h"
#include "init/ecs.h"
#include "stdinLuaConsole.h"

#include "graphics/opengl.h"

#include "hardware/hardwareController.h"
#if WITH_DISCORD
#include "discord.h"
#endif
#if STEAMSDK
#include "steam/steam_api.h"
#include "steamrichpresence.h"
#endif

#include "shaderRegistry.h"
#include "glObjects.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <unordered_map>
#endif

glm::vec3 camera_position;
float camera_yaw;
float camera_pitch;
sp::Font* main_font;
sp::Font* bold_font;
RenderLayer* consoleRenderLayer;
RenderLayer* mouseLayer;
PostProcessor* glitchPostProcessor;
PostProcessor* warpPostProcessor;
PVector<Window> windows;
std::vector<RenderLayer*> window_render_layers;

#include "gui/layout/vertical.h"
#include "gui/layout/horizontal.h"
GUI_REGISTER_LAYOUT("default", GuiLayout);
GUI_REGISTER_LAYOUT("vertical", GuiLayoutVertical);
GUI_REGISTER_LAYOUT("verticalbottom", GuiLayoutVerticalBottom);
GUI_REGISTER_LAYOUT("horizontal", GuiLayoutHorizontal);
GUI_REGISTER_LAYOUT("horizontalright", GuiLayoutHorizontalRight);

#ifdef __EMSCRIPTEN__
static void browserDiag(const string& message)
{
    EM_ASM({
        if (typeof window.EmptyEpsilonDiag === "function")
            window.EmptyEpsilonDiag(UTF8ToString($0));
    }, message.c_str());
}

namespace
{
    struct BrowserScenarioAssetRequestState
    {
        bool ready = false;
        bool failed = false;
        string error;
        string status;
    };

    std::unordered_map<string, BrowserScenarioAssetRequestState> browserScenarioAssetRequests;

    EM_JS(int, ee_browser_request_ready, (const char* scenario_filename), {
        var state = window.EmptyEpsilonBrowserScenarioRequestState;
        var entry = state ? state[UTF8ToString(scenario_filename)] : null;
        return entry && entry.ready ? 1 : 0;
    });

    EM_JS(int, ee_browser_request_failed, (const char* scenario_filename), {
        var state = window.EmptyEpsilonBrowserScenarioRequestState;
        var entry = state ? state[UTF8ToString(scenario_filename)] : null;
        return entry && entry.failed ? 1 : 0;
    });

    EM_JS(char*, ee_browser_request_error, (const char* scenario_filename), {
        var state = window.EmptyEpsilonBrowserScenarioRequestState;
        var entry = state ? state[UTF8ToString(scenario_filename)] : null;
        return stringToNewUTF8(entry && entry.error ? String(entry.error) : "");
    });

    EM_JS(char*, ee_browser_request_status, (const char* scenario_filename), {
        var state = window.EmptyEpsilonBrowserScenarioRequestState;
        var entry = state ? state[UTF8ToString(scenario_filename)] : null;
        return stringToNewUTF8(entry && entry.status ? String(entry.status) : "");
    });
}

void requestBrowserScenarioAssets(const string& scenario_filename, bool local_session)
{
    auto& state = browserScenarioAssetRequests[scenario_filename];
    state.ready = false;
    state.failed = false;
    state.error = "";
    state.status = "Requesting browser asset load";
    browserDiag("browser assets: requested for " + scenario_filename + (local_session ? " (local)" : " (remote)"));
    EM_ASM({
        try {
            if (window.EmptyEpsilonAssetLoader && typeof window.EmptyEpsilonAssetLoader.requestScenarioAssets === "function") {
                window.EmptyEpsilonAssetLoader.requestScenarioAssets(UTF8ToString($0), $1 ? true : false);
                return;
            }
            window.EmptyEpsilonBrowserScenarioRequestState = window.EmptyEpsilonBrowserScenarioRequestState || Object.create(null);
            var unavailableEntry = {};
            unavailableEntry.ready = false;
            unavailableEntry.failed = true;
            unavailableEntry.error = "Browser asset loader unavailable";
            unavailableEntry.status = "Failed: Browser asset loader unavailable";
            window.EmptyEpsilonBrowserScenarioRequestState[UTF8ToString($0)] = unavailableEntry;
        } catch (error) {
            window.EmptyEpsilonBrowserScenarioRequestState = window.EmptyEpsilonBrowserScenarioRequestState || Object.create(null);
            var failedEntry = {};
            failedEntry.ready = false;
            failedEntry.failed = true;
            failedEntry.error = String(error || "Browser asset request failed");
            failedEntry.status = "Failed: " + String(error || "Browser asset request failed");
            window.EmptyEpsilonBrowserScenarioRequestState[UTF8ToString($0)] = failedEntry;
        }
    }, scenario_filename.c_str(), local_session ? 1 : 0);
}

bool browserScenarioAssetsReady(const string& scenario_filename)
{
#ifdef __EMSCRIPTEN__
    if (ee_browser_request_ready(scenario_filename.c_str()))
        return true;
#endif
    auto it = browserScenarioAssetRequests.find(scenario_filename);
    return it != browserScenarioAssetRequests.end() && it->second.ready;
}

bool browserScenarioAssetsFailed(const string& scenario_filename)
{
#ifdef __EMSCRIPTEN__
    if (ee_browser_request_failed(scenario_filename.c_str()))
        return true;
#endif
    auto it = browserScenarioAssetRequests.find(scenario_filename);
    return it != browserScenarioAssetRequests.end() && it->second.failed;
}

string browserScenarioAssetsError(const string& scenario_filename)
{
#ifdef __EMSCRIPTEN__
    char* script_result = ee_browser_request_error(scenario_filename.c_str());
    if (script_result)
    {
        string result = script_result;
        std::free(script_result);
        if (!result.empty())
            return result;
    }
#endif
    auto it = browserScenarioAssetRequests.find(scenario_filename);
    if (it == browserScenarioAssetRequests.end())
        return "";
    return it->second.error;
}

string browserScenarioAssetsStatus(const string& scenario_filename)
{
#ifdef __EMSCRIPTEN__
    char* script_result = ee_browser_request_status(scenario_filename.c_str());
    if (script_result)
    {
        string result = script_result;
        std::free(script_result);
        if (!result.empty())
            return result;
    }
#endif
    auto it = browserScenarioAssetRequests.find(scenario_filename);
    if (it == browserScenarioAssetRequests.end())
        return "";
    return it->second.status;
}

void clearBrowserScenarioAssetRequest(const string& scenario_filename)
{
    browserScenarioAssetRequests.erase(scenario_filename);
    EM_ASM({
        if (window.EmptyEpsilonBrowserScenarioRequestState)
            delete window.EmptyEpsilonBrowserScenarioRequestState[UTF8ToString($0)];
    }, scenario_filename.c_str());
}

void notifyBrowserRemoteSessionManifestChanged(const string& manifest_url, const string& revision)
{
    EM_ASM({
        if (typeof window.EmptyEpsilonOnRemoteSessionManifestChanged === "function")
            window.EmptyEpsilonOnRemoteSessionManifestChanged(UTF8ToString($0), UTF8ToString($1));
    }, manifest_url.c_str(), revision.c_str());
}
#endif

int runProxyServer()
{
    int port = defaultServerPort;
    string password = "";
    int listenPort = defaultServerPort;
    string proxyName = "";
    auto parts = PreferencesManager::get("proxy").split(":");
    string host = parts[0];
    if (parts.size() > 1) port = parts[1].toInt();
    if (parts.size() > 2) password = parts[2].upper();
    if (parts.size() > 3) listenPort = parts[3].toInt();
    if (parts.size() > 4) proxyName = parts[4];
    if (host == "listen")
        new GameServerProxy(password, listenPort, proxyName);
    else
        new GameServerProxy(host, port, password, listenPort, proxyName);
    engine->runMainLoop();
    return 0;
}

int main(int argc, char** argv)
{
#ifdef DEBUG
    Logging::setLogLevel(LOGLEVEL_DEBUG);
#else
    Logging::setLogLevel(LOGLEVEL_INFO);
#endif

// Log to STDOUT unless on non-debug Windows builds, which won't have
// terminals for log output.
#if defined(_WIN32) && !defined(DEBUG)
    Logging::setLogFile("EmptyEpsilon.log");
#else
    Logging::setLogStdout();
#endif

    LOG(Info, "Starting...");
#ifdef __EMSCRIPTEN__
    browserDiag("main: startup");
#endif
    new Engine();
    initSystemsAndComponents();

    auto configuration_path = initConfiguration(argc, argv);
#ifdef __EMSCRIPTEN__
    browserDiag("main: config path = " + configuration_path);
#endif

    if (PreferencesManager::get("headless") == "")
    {
#ifdef _WIN32
        mkdir(configuration_path.c_str());
#else
        mkdir(configuration_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
// On macOS non-debug builds, redirect the log to the configuration directory if
// invoked as an app bundle.
#ifdef __APPLE__
        const char* argv0 = *argv;
        std::string launch_path(argv0);

        // Check if the path ends with .app/Contents/MacOS/
        // If so, we're invoked as an app bundle. Write the log to file.
        size_t pos = launch_path.find(".app/Contents/MacOS/");
        if (pos != std::string::npos)
            Logging::setLogFile(configuration_path + "/EmptyEpsilon.log");
        // If not, we might be invoked as a binary and can log to STDOUT.
        else Logging::setLogStdout();
#endif // __APPLE__

#endif // _WIN32
    }

    if (PreferencesManager::get("proxy") != "")
        return runProxyServer();

    if (PreferencesManager::get("headless") != "")
    {
        textureManager.setDisabled(true);
        Logging::setLogStdout();
    }

    initResourcePaths();
#ifdef __EMSCRIPTEN__
    browserDiag("main: resource paths initialized");
#endif
    textureManager.setDefaultSmooth(true);
    textureManager.setDefaultRepeated(true);
    i18n::load("locale/main." + PreferencesManager::get("language", "en") + ".po");
    keys.init();
#ifdef __EMSCRIPTEN__
    keys.voice_all.addKey("virtual:250");
    keys.voice_ship.addKey("virtual:251");
    keys.escape.addKey("virtual:252");
#endif
    if (PreferencesManager::get("httpserver").toInt() != 0)
    {
        int port_nr = PreferencesManager::get("httpserver").toInt();
        if (port_nr < 10)
            port_nr = 80;
        LOG(INFO) << "Enabling HTTP script access on port: " << port_nr;
        LOG(INFO) << "NOTE: This is potentially a risk!";
        new EEHttpServer(port_nr, PreferencesManager::get("www_directory", "www"));
    }

    string theme_name = PreferencesManager::get("guitheme", "default");
    if (!GuiTheme::loadTheme(theme_name, "gui/" + theme_name + ".theme.txt"))
    {
        LOG(Error, "Failed to load " + theme_name + " theme, trying default. Resources missing or contains errors? Check gui/" + theme_name + ".theme.txt");
        if (!GuiTheme::loadTheme("default", "gui/default.theme.txt"))
        {
            LOG(Error, "Failed to load default theme, exiting. Check gui/default.theme.txt"); //Yes, we may try to load twice default theme but this should be a rare error case which always finish in exit
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to load gui theme, resources missing or contains errors? Check gui/default.theme.txt", nullptr);
            return 1;
        }
        GuiTheme::setCurrentTheme("default");
    }
    else
    {
        GuiTheme::setCurrentTheme(theme_name);
    }

    if (PreferencesManager::get("headless") == "")
    {
        if (!createDisplayWindows())
            return 1;
#ifdef __EMSCRIPTEN__
        browserDiag("main: display windows created");
#endif
    } else {
        new StdinLuaConsole();
    }

    soundManager->setMusicVolume(PreferencesManager::get("music_volume", "50").toFloat());
    soundManager->setMasterSoundVolume(PreferencesManager::get("sound_volume", "50").toFloat());

    const auto& active_theme = GuiTheme::getCurrentTheme();
    main_font = active_theme->getStyle("base")->get(GuiElement::State::Normal).font;
    bold_font = active_theme->getStyle("bold")->get(GuiElement::State::Normal).font;
    if (!main_font || !bold_font)
    {
        LOG(ERROR, "Missing font or bold font.");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to load main or bold font, resources missing?", nullptr);
        return 1;
    }

    sp::RenderTarget::setDefaultFont(main_font);

    // Apply baseline offset adjustments to fonts
    // Positive values move text down, negative values move text up
    main_font->setBaselineOffset(active_theme->getStyle("base")->get(GuiElement::State::Normal).font_offset);
    bold_font->setBaselineOffset(active_theme->getStyle("bold")->get(GuiElement::State::Normal).font_offset);

    // On Android, this requires the 'record audio' permissions,
    // which is always a scary thing for users.
    // Since there is no way to access it (yet) via a touchscreen, compile out.
#if !defined(ANDROID)
    // Set up voice chat and key bindings.
    const auto voice_chat_default =
#ifdef __EMSCRIPTEN__
        "1";
#else
        "0";
#endif
    if (PreferencesManager::get("voice_chat_enabled", voice_chat_default) == "1")
    {
        NetworkAudioRecorder* nar = new NetworkAudioRecorder();
        nar->addKeyActivation(&keys.voice_all, 0);
        nar->addKeyActivation(&keys.voice_ship, 1);
    }
#endif

    P<HardwareController> hardware_controller = new HardwareController();
#if defined(__EMSCRIPTEN__)
    (void)hardware_controller;
    LOG(Info, "Browser build: skipping external hardware configuration.");
#else
    hardware_controller->loadConfiguration(configuration_path + "/hardware.ini");
#endif

#if WITH_DISCORD
    {
        std::filesystem::path discord_sdk{
#ifdef RESOURCE_BASE_DIR
        RESOURCE_BASE_DIR
#endif
        };
        discord_sdk /= std::filesystem::path{ "plugins" } / DynamicLibrary::add_native_suffix("discord_game_sdk");
        new DiscordRichPresence(discord_sdk);
    }
#endif // WITH_DISCORD
#if STEAMSDK
    new SteamRichPresence();
#endif //STEAMSDK

    string tutorial = PreferencesManager::get("tutorial");   // use "00_all.lua" for all tutorials
    string server_scenario = PreferencesManager::get("server_scenario");
    string browser_connect = PreferencesManager::get("browser_connect");

    if (!tutorial.empty())
    {
        bool repeat_tutorial = PreferencesManager::get("repeat_tutorial", "false") == "true";
        LOG(DEBUG) << "Starting tutorial: " << tutorial;
        new TutorialGame(repeat_tutorial, tutorial);
    }
    else if (server_scenario.empty())
    {
#ifdef __EMSCRIPTEN__
        if (!browser_connect.empty())
        {
            LOG(Info, "Starting browser websocket bridge connect flow: ", browser_connect);
            PreferencesManager::set("browser_bridge_url", browser_connect);
            browserDiag("main: browser bridge connect requested");

            ServerScanner::ServerInfo info;
            info.type = ServerScanner::ServerType::Manual;
            info.name = browser_connect;
            new JoinServerScreen(info);
            browserDiag("main: join server screen created");
        }
        else if (PreferencesManager::get("browser_bootstrap", "1") != "0")
        {
            LOG(Info, "Starting browser bootstrap scenario in spectator mode.");
            browserDiag("main: browser bootstrap enabled");
            new EpsilonServer(defaultServerPort, false);
            if (!gameGlobalInfo)
                return 1;
            browserDiag("main: local server created");
            gameGlobalInfo->startScenario(PreferencesManager::get("browser_scenario", "scenario_00_basic.lua"), loadScenarioSettingsFromPrefs());
            browserDiag("main: scenario started");
            new SpectatorScreen(defaultRenderLayer);
            browserDiag("main: spectator screen created");
        }
        else
#endif
        {
            returnToMainMenu(defaultRenderLayer);
        }
    }
    else
    {
        // server_scenario creates a server running the specified scenario
        // using its defined default settings, and launches directly into
        // the ship selection screen instead of the main menu.

        // Create the server to listen on the assigned port.
        // Use the default port if server_port isn't set or has an invalid
        // value (toInt returns 0 if empty or not an int).
        int server_port = PreferencesManager::get("server_port").toInt();

        if (server_port < 10 || server_port > 65535)
        {
            LOG(Warning, "Invalid server_port " + string(server_port));
            server_port = defaultServerPort;
        }

        LOG(Info, "Launching server_scenario " + server_scenario + " on port " + string(server_port));
        new EpsilonServer(server_port);

        if(!gameGlobalInfo) // => failed to start server
            return 1;

        if (PreferencesManager::get("server_name") != "") game_server->setServerName(PreferencesManager::get("server_name"));
        if (PreferencesManager::get("server_password") != "") game_server->setPassword(PreferencesManager::get("server_password").upper());
        if (PreferencesManager::get("server_internet") == "1") game_server->registerOnMasterServer(PreferencesManager::get("registry_registration_url", "http://daid.eu/ee/register.php"));

        // Load the scenario and open the ship selection screen.
        gameGlobalInfo->startScenario(server_scenario, loadScenarioSettingsFromPrefs());
        new ShipSelectionScreen();
    }

    engine->runMainLoop();

    // Set FSAA and fullscreen defaults from windowManager.
    if (windows.size() > 0)
    {
        PreferencesManager::set("fsaa", windows[0]->getFSAA());
        PreferencesManager::set("fullscreen", (int)windows[0]->getMode());
    }

    // Set the default music_, sound_, and engine_volume to the current volume.
    PreferencesManager::set("music_volume", soundManager->getMusicVolume());
    PreferencesManager::set("sound_volume", soundManager->getMasterSoundVolume());
    PreferencesManager::set("engine_volume", PreferencesManager::get("engine_volume", "50"));

    // Enable music and engine sounds on the main screen only by default.
    if (PreferencesManager::get("music_enabled").empty())
        PreferencesManager::set("music_enabled", "2");

    if (PreferencesManager::get("engine_enabled").empty())
        PreferencesManager::set("engine_enabled", "2");

    if (PreferencesManager::get("headless") == "")
    {
        saveConfiguration(configuration_path);
    }
    windows.clear();
    delete engine;

    return 0;
}

void returnToMainMenu(RenderLayer* render_layer)
{
#ifdef __EMSCRIPTEN__
    PreferencesManager::set("browser_local_session", "");
#endif
    if (render_layer != defaultRenderLayer) // Handle secondary monitors
    {
        returnToShipSelection(render_layer);
        return;
    }

    string headless = PreferencesManager::get("headless", "");
    if (!headless.empty())
    {
        // Create the server to listen on the assigned port.
        // Use the default port if server_port isn't set or has an invalid
        // value (toInt returns 0).
        int headless_port = PreferencesManager::get("server_port").toInt();
        // This is the same process as server_port and could be made DRY.
        if (headless_port < 10 || headless_port > 65535)
        {
            LOG(Warning, "Invalid server_port: " + string(headless_port));
            headless_port = defaultServerPort;
        }

        LOG(Info, "Launching headless scenario " + headless + " on port " + string(headless_port));
        new EpsilonServer(headless_port);

        if (PreferencesManager::get("headless_name") != "") game_server->setServerName(PreferencesManager::get("headless_name"));
        if (PreferencesManager::get("headless_password") != "") game_server->setPassword(PreferencesManager::get("headless_password").upper());
        if (PreferencesManager::get("headless_internet") == "1") game_server->registerOnMasterServer(PreferencesManager::get("registry_registration_url", "http://daid.eu/ee/register.php"));
        gameGlobalInfo->startScenario(headless, loadScenarioSettingsFromPrefs());

        if (PreferencesManager::get("startpaused") != "1")
            engine->setGameSpeed(1.0);
    }
    else if (!PreferencesManager::get("autoconnect").empty())
    {
        auto value = PreferencesManager::get("autoconnect");

        std::vector<AutoConnectPosition> window_positions;
        for (auto part : value.split(";"))
            window_positions.push_back(AutoConnectPosition(part));

        new AutoConnectScreen(window_positions, PreferencesManager::get("autocontrolmainscreen").toInt(), PreferencesManager::get("autoconnectship", "solo"));
    }
    else
    {
        new MainMenu();
    }
}

void returnToShipSelection(RenderLayer* render_layer)
{
    if (render_layer != defaultRenderLayer)
    {
        for(size_t n=0; n<window_render_layers.size(); n++)
            if (window_render_layers[n] == render_layer)
                new SecondMonitorScreen(n);
    } else {
        if (PreferencesManager::get("autoconnect") != "")
        {
            // Preserve autoconnect for startup, but once the client is already
            // connected let Escape return to the normal ship/role picker.
            if (game_client && game_client->getStatus() == GameClient::Connected)
                new ShipSelectionScreen();
            else
                returnToMainMenu(render_layer);
        }
        else
        {
            new ShipSelectionScreen();
        }
    }
}

void returnToOptionMenu(OptionsMenu::ReturnTo return_to)
{
    new OptionsMenu(return_to);
}

std::unordered_map<string, string> loadScenarioSettingsFromPrefs()
{
    string preferenceValue = PreferencesManager::get("scenario_settings");

    std::unordered_map<string, string> settings = {};
    if (preferenceValue == "")
        return settings;

    for(string setting : preferenceValue.split(";"))
    {
        auto [key, value] = setting.partition("=");
        if (!key.empty() && !value.empty())
            settings[key.strip()] = value.strip();
    }

    return settings;
}
