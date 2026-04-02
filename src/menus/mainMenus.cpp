#include <i18n.h>
#include "engine.h"
#include "mainMenus.h"
#include "main.h"
#include "preferenceManager.h"
#include "epsilonServer.h"
#include "playerInfo.h"
#include "gameGlobalInfo.h"
#include "menus/serverCreationScreen.h"
#include "menus/optionsMenu.h"
#include "menus/tutorialMenu.h"
#include "menus/serverBrowseMenu.h"
#include "screens/gm/gameMasterScreen.h"
#include "screenComponents/rotatingModelView.h"
#include "config.h"

#include "gui/theme.h"
#include "gui/gui2_image.h"
#include "gui/gui2_label.h"
#include "gui/gui2_button.h"
#include "gui/gui2_textentry.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __EMSCRIPTEN__
namespace
{
    float browserMainMenuTouchScale()
    {
        const char* script_result = emscripten_run_script_string(
            "(function(){"
            "var coarse = !!(window.matchMedia && window.matchMedia('(pointer: coarse)').matches);"
            "var touch = coarse || ((navigator.maxTouchPoints || 0) > 0);"
            "if (!touch) return '1';"
            "var shortest = Math.min(window.innerWidth || 0, window.innerHeight || 0);"
            "if (shortest > 0 && shortest <= 430) return '1.95';"
            "if (shortest > 0 && shortest <= 820) return '1.35';"
            "return '1.0';"
            "})()"
        );
        return script_result ? string{script_result}.toFloat() : 1.0f;
    }
}
#endif

MainMenu::MainMenu()
{
    constexpr float logo_size = 256;
    constexpr float logo_size_y = 256;
    constexpr float logo_size_x = 1024;
    constexpr float title_y = 160;

    float menu_scale = 1.0f;
#ifdef __EMSCRIPTEN__
    menu_scale = browserMainMenuTouchScale();
#endif
    const float label_width = 300.0f * menu_scale;
    const float field_width = 300.0f * menu_scale;
    const float button_width = 300.0f * menu_scale;
    const float control_height = 50.0f * menu_scale;
    const float label_size = 30.0f * menu_scale;
    const float button_text_size = 24.0f * menu_scale;
    const float left_margin = 50.0f;
    const float name_label_y = -400.0f - (control_height - 50.0f) * 0.5f;
    const float name_entry_y = name_label_y + control_height + 10.0f;
    const float start_server_y = name_entry_y + 120.0f;
    const float start_client_y = start_server_y + control_height + 10.0f;
    const float options_y = start_client_y + control_height + 10.0f;
    const float tutorials_y = options_y + control_height + 10.0f;

    new GuiOverlay(this, "", GuiTheme::getColor("background"));
    (new GuiOverlay(this, "", glm::u8vec4{255,255,255,255}))->setTextureTiledThemed("background.crosses");

    (new GuiImage(this, "LOGO", "logo_full.png"))->setPosition(0, title_y, sp::Alignment::TopCenter)->setSize(logo_size_x, logo_size_y);
    (new GuiLabel(this, "VERSION", tr("Credits", "Version: {version}").format({{"version", string(VERSION_NUMBER)}}), 20))->setPosition(0, title_y + logo_size, sp::Alignment::TopCenter)->setSize(0, 20);

    (new GuiLabel(this, "", tr("mainMenu", "Your name:"), label_size))->setAlignment(sp::Alignment::CenterLeft)->setPosition({left_margin, name_label_y}, sp::Alignment::BottomLeft)->setSize(label_width, control_height);
    auto username_entry = new GuiTextEntry(this, "USERNAME", PreferencesManager::get("username"));
    username_entry->callback([](string text) {
        PreferencesManager::set("username", text);
    });
    username_entry->setPosition({left_margin, name_entry_y}, sp::Alignment::BottomLeft)->setSize(field_width, control_height);
    username_entry->setTextSize(label_size);

    string start_server_label = tr("mainMenu", "Start server");
#ifdef __EMSCRIPTEN__
    start_server_label = tr("mainMenu", "Start local session");
#endif
    auto start_server_button = new GuiButton(this, "START_SERVER", start_server_label, [this]() {
#ifdef __EMSCRIPTEN__
        PreferencesManager::set("browser_local_session", "1");
        new EpsilonServer(defaultServerPort, false);
        if (game_server)
        {
            game_server->setServerName("Local Session");
            new ServerScenarioSelectionScreen();
            destroy();
        }
#else
        new ServerSetupScreen();
        destroy();
#endif
    });
    start_server_button->setPosition({left_margin, start_server_y}, sp::Alignment::BottomLeft)->setSize(button_width, control_height);
    start_server_button->setTextSize(button_text_size);

    string start_client_label = tr("mainMenu", "Start client");
#ifdef __EMSCRIPTEN__
    start_client_label = tr("mainMenu", "Join server (WebSocket)");
#endif
    auto start_client_button = new GuiButton(this, "START_CLIENT", start_client_label, [this]() {
        new ServerBrowserMenu();
        destroy();
    });
    start_client_button->setPosition({left_margin, start_client_y}, sp::Alignment::BottomLeft)->setSize(button_width, control_height);
    start_client_button->setTextSize(button_text_size);

    auto options_button = new GuiButton(this, "OPEN_OPTIONS", tr("mainMenu", "Options"), [this]() {
        new OptionsMenu(OptionsMenu::ReturnTo::Main);
        destroy();
    });
    options_button->setPosition({left_margin, options_y}, sp::Alignment::BottomLeft)->setSize(button_width, control_height);
    options_button->setTextSize(button_text_size);

#ifndef __EMSCRIPTEN__
    (new GuiButton(this, "QUIT", tr("mainMenu", "Quit"), []() {
        engine->shutdown();
    }))->setPosition({50, -50}, sp::Alignment::BottomLeft)->setSize(300, 50);
#endif

    glm::vec2 tutorials_position = {370, -50};
#ifdef __EMSCRIPTEN__
    tutorials_position = {left_margin, tutorials_y};
#endif
    auto tutorials_button = new GuiButton(this, "START_TUTORIAL", tr("mainMenu", "Tutorials"), [this]() {
        new TutorialMenu();
        destroy();
    });
    tutorials_button->setPosition(tutorials_position, sp::Alignment::BottomLeft)->setSize(button_width, control_height);
    tutorials_button->setTextSize(button_text_size);

    float y = 100;
    (new GuiLabel(this, "CREDITS", tr("Credits", "Credits"), 25))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 25); y += 25;
    (new GuiLabel(this, "CREDITS1", tr("Credits", "Programming:"), 20))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 20); y += 20;
    (new GuiLabel(this, "CREDITS2", "Daid", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS2", "gcask", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS2", "Nallath", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS2", "Xansta", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS2", "StarryWisdom", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    y += 10;
    (new GuiLabel(this, "CREDITS1", tr("Credits", "Graphics:"), 20))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 20); y += 20;
    (new GuiLabel(this, "CREDITS3", "Interesting John", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    y += 10;
    (new GuiLabel(this, "CREDITS1", tr("Credits", "Localization:"), 20))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 20); y += 20;
    (new GuiLabel(this, "CREDITS3", "Muerte (FR)", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS3", "aBlueShadow (DE)", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    y += 10;
    (new GuiLabel(this, "CREDITS4", tr("Credits", "Music:"), 20))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 20); y += 20;
    (new GuiLabel(this, "CREDITS5", "Matthew Pablo", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS6", "Alexandr Zhelanov", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS7", "Joe Baxter-Webb", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS8", "neocrey", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS9", "FoxSynergy", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    y += 10;
    (new GuiLabel(this, "CREDITS10", tr("Credits", "Models:"), 20))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 20); y += 20;
    (new GuiLabel(this, "CREDITS11", "Angryfly (turbosquid.com)", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS12", "SolCommand (http://solcommand.blogspot.com/)", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    y += 10;
    (new GuiLabel(this, "CREDITS13", tr("Credits", "Crew sprites:"), 20))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 20); y += 20;
    (new GuiLabel(this, "CREDITS14", "Tokka (http://bekeen.de/)", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    y += 10;
    (new GuiLabel(this, "CREDITS15", tr("Credits", "Special thanks:"), 20))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 20); y += 20;
    (new GuiLabel(this, "CREDITS16", "Marty Lewis (MadKat)", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS17", "Serge Wroclawski", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS18", "Dennis Shelton", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS19", "VolgClawtooth", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS20", "Daniel Loftis", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS21", "David Concepcion", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS22", "Philippe Bruylant", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS23", "Ralf Leichter", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS24", "Lee McDonough (Flea)", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;
    (new GuiLabel(this, "CREDITS25", "Mickael Houet", 18))->setAlignment(sp::Alignment::CenterRight)->setPosition(-50, y, sp::Alignment::TopRight)->setSize(0, 18); y += 18;

    if (PreferencesManager::get("instance_name") != "")
    {
        (new GuiLabel(this, "", PreferencesManager::get("instance_name"), 25))->setAlignment(sp::Alignment::CenterLeft)->setPosition(20, 20, sp::Alignment::TopLeft)->setSize(0, 18);
    }

#ifdef DEBUG
    (new GuiButton(this, "", "TO DA GM!", [this]() {
        new EpsilonServer(defaultServerPort);
        if (game_server)
        {
            gameGlobalInfo->startScenario("scenario_10_empty.lua");

            my_player_info->commandSetShip({});
            destroy();
            new GameMasterScreen(nullptr);
        }
    }))->setPosition({370, -150}, sp::Alignment::BottomLeft)->setSize(300, 50);
#endif
}
