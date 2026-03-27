#include "config.h"
#include "engine.h"
#include <stringImproved.h>
#include <io/keybinding.h>
#include <preferenceManager.h>
#include "gui/hotkeyConfig.h"
#include "soundManager.h"
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#if STEAMSDK
#include "steam/steam_api.h"
#include "steamrichpresence.h"
#endif

namespace
{
    string last_configuration_path;
}

string initConfiguration(int argc, char** argv)
{
    string configuration_path = ".";
#ifdef __EMSCRIPTEN__
    configuration_path = "/config";
    string browser_profile;
    for(int n=1; n<argc; n++)
    {
        string argument = argv[n];
        auto parts = argument.split("=");
        if (parts.size() == 2 && parts[0].strip() == "browser_profile")
        {
            browser_profile = parts[1].strip().lower();
            break;
        }
    }
    if (!browser_profile.empty())
    {
        string safe_profile;
        for (char ch : browser_profile)
        {
            if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.')
                safe_profile += ch;
            else
                safe_profile += '-';
        }
        safe_profile = safe_profile.strip('-');
        if (!safe_profile.empty())
            configuration_path = "/config-" + safe_profile;
    }
#else
    if (getenv("EE_CONF_DIR"))
        configuration_path = string(getenv("EE_CONF_DIR"));
    else if (getenv("HOME"))
        configuration_path = string(getenv("HOME")) + "/.emptyepsilon";
#endif
#ifdef STEAMSDK
    {
        char path_buffer[1024];
        if (SteamUser()->GetUserDataFolder(path_buffer, sizeof(path_buffer)))
            configuration_path = path_buffer;
    }
#endif
    LOG(Info, "Using ", configuration_path, " as configuration path");
    PreferencesManager::load(configuration_path + "/options.ini");

    for(int n=1; n<argc; n++)
    {
        char* value = strchr(argv[n], '=');
        if (!value) continue;
        *value++ = '\0';
        PreferencesManager::setTemporary(string(argv[n]).strip(), string(value).strip());
    }

    if (PreferencesManager::get("username", "") == "")
    {
#ifdef STEAMSDK
        PreferencesManager::setTemporary("username", SteamFriends()->GetPersonaName());
#else
        if (getenv("USERNAME"))
            PreferencesManager::setTemporary("username", getenv("USERNAME"));
        else if (getenv("USER"))
            PreferencesManager::setTemporary("username", getenv("USER"));
#ifdef __EMSCRIPTEN__
        else
            PreferencesManager::setTemporary("username", "Browser");
#endif
#endif
    }

    sp::io::Keybinding::loadKeybindings(configuration_path + "/keybindings.json");
    last_configuration_path = configuration_path;
    return configuration_path;
}

void saveConfiguration(const string& configuration_path)
{
    PreferencesManager::save(configuration_path + "/options.ini");
    sp::io::Keybinding::saveKeybindings(configuration_path + "/keybindings.json");
#ifdef __EMSCRIPTEN__
    EM_ASM({
        if (typeof FS !== "undefined" && FS.filesystems && FS.filesystems.IDBFS)
        {
            FS.syncfs(false, function(err) {
                if (typeof window.EmptyEpsilonDiag === "function")
                    window.EmptyEpsilonDiag(err ? "idbfs: flush failed" : "idbfs: flush complete");
            });
        }
    });
#endif
}

#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void ee_browser_save_configuration()
{
    if (last_configuration_path.empty())
        return;
    saveConfiguration(last_configuration_path);
}

extern "C" EMSCRIPTEN_KEEPALIVE int ee_browser_ensure_audio_started()
{
    if (!engine)
        return 0;
    return engine->ensureAudioStarted() ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE void ee_browser_play_test_sound()
{
    if (!soundManager)
        return;
    soundManager->playSound("sfx/button.wav");
}
#endif
