#include "main.h"
#include "epsilonServer.h"
#include "menus/joinServerMenu.h"
#include "menus/serverBrowseMenu.h"
#include "playerInfo.h"
#include "preferenceManager.h"
#include "gameGlobalInfo.h"
#include "i18n.h"
#include "config.h"
#include "gui/gui2_label.h"
#include "gui/gui2_panel.h"

#include "gui/gui2_textentry.h"
#include "gui/gui2_button.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
namespace {
void browserDiag(const string& message)
{
    EM_ASM({
        if (typeof window.EmptyEpsilonDiag === "function")
            window.EmptyEpsilonDiag(UTF8ToString($0));
    }, message.c_str());
}

string browserStationLabel()
{
    auto station = PreferencesManager::get("browser_station", "").lower().strip();
    if (station.empty())
        return "none";
    if (station == "mainscreen" || station == "main")
        return "main screen";
    return station;
}
}
#endif

JoinServerScreen::JoinServerScreen(const ServerScanner::ServerInfo& target)
: target(target)
{
#ifdef __EMSCRIPTEN__
    browserDiag("join: ctor target = " + target.name);
    status_label = new GuiLabel(this, "STATUS", "Connecting to websocket bridge...", 30);
    status_label->setPosition(0, 260, sp::Alignment::TopCenter)->setSize(900, 60);
    browser_info_label = new GuiLabel(this, "STATUS_INFO",
        "Browser multiplayer currently expects a websocket bridge that proxies\n"
        "to a native EmptyEpsilon server. Raw TCP/UDP is still desktop-only.",
        24);
    browser_info_label->setPosition(0, 340, sp::Alignment::TopCenter)->setSize(900, 120);
    browser_info_label->setText(
        "Preferred station: " + browserStationLabel() + "\n"
        "The browser client will try to claim the first available player ship\n"
        "and launch that station after replication catches up."
    );
    (new GuiButton(this, "BTN_CANCEL", tr("button", "Back"), [this]() {
        destroy();
        disconnectFromServer();
        new ServerBrowserMenu();
    }))->setPosition(50, -50, sp::Alignment::BottomLeft)->setSize(300, 50);
    new GameClient(VERSION_NUMBER, target.name);
    browserDiag("join: game client created");
    return;
#endif

    status_label = new GuiLabel(this, "STATUS", tr("connectserver", "Connecting..."), 30);
    status_label->setPosition(0, 300, sp::Alignment::TopCenter)->setSize(0, 50);
    (new GuiButton(this, "BTN_CANCEL", tr("button", "Cancel"), [this]() {
        destroy();
        disconnectFromServer();
        new ServerBrowserMenu();
    }))->setPosition(50, -50, sp::Alignment::BottomLeft)->setSize(300, 50);

    password_entry_box = new GuiPanel(this, "PASSWORD_ENTRY_BOX");
    password_entry_box->setPosition(0, 350, sp::Alignment::TopCenter)->setSize(600, 100);
    password_entry_box->hide();
    password_entry = new GuiTextEntry(password_entry_box, "PASSWORD_ENTRY", "");
    password_entry->setPosition(20, 0, sp::Alignment::CenterLeft)->setSize(400, 50);
    password_entry->setHidePassword();
    password_entry->enterCallback([this](string entry)
    {
        password_entry_box->hide();
        password_focused = false;
        game_client->sendPassword(entry.upper());
    });
    (new GuiButton(password_entry_box, "PASSWORD_ENTRY_OK", "Ok", [this]()
    {
        password_entry_box->hide();
        password_focused = false;
        game_client->sendPassword(password_entry->getText().upper());
    }))->setPosition(420, 0, sp::Alignment::CenterLeft)->setSize(160, 50);

    if (target.type == ServerScanner::ServerType::SteamFriend) {
#ifdef STEAMSDK
        new GameClient(VERSION_NUMBER, target.port);
#endif
    } else {
        new GameClient(VERSION_NUMBER, target.address, target.port);
    }
}

void JoinServerScreen::update(float delta)
{
    (void)delta;
    switch(game_client->getStatus())
    {
    case GameClient::Connecting:
    case GameClient::Authenticating:
#ifdef __EMSCRIPTEN__
        status_label->setText("Connecting to websocket bridge...");
        if (browser_info_label)
            browser_info_label->setText(
                "Preferred station: " + browserStationLabel() + "\n"
                "Waiting for websocket connection and server authentication."
            );
#endif
        //If we are still trying to connect, do nothing.
        break;
    case GameClient::WaitingForPassword:
        status_label->setText(tr("Please enter the server password:"));
        password_entry_box->show();
        if (!password_focused)
        {
            password_focused = true;
            focus(password_entry);
        }
        break;
    case GameClient::Disconnected: {
#ifdef __EMSCRIPTEN__
        browserDiag("join: disconnected reason = " + string(static_cast<int>(game_client->getDisconnectReason())));
#endif
        auto reason = game_client->getDisconnectReason();
        destroy();
        disconnectFromServer();
        
        new ServerBrowserMenu(reason);
        } break;
    case GameClient::Connected:
#ifdef __EMSCRIPTEN__
        browserDiag("join: game client connected");
        status_label->setText("Connected to websocket bridge");
        if (browser_info_label)
            browser_info_label->setText(
                "Preferred station: " + browserStationLabel() + "\n"
                "Connected. Waiting for player state and ship replication."
            );
#endif
        if (!target.address.getHumanReadable().empty())
        {
            string last_server = target.address.getHumanReadable()[0];
            if (target.port != defaultServerPort)
                last_server += ":" + string(int(target.port));
            PreferencesManager::set("last_server", last_server);
        }
        if (game_client->getClientId() > 0)
        {
            foreach(PlayerInfo, i, player_info_list)
                if (i->client_id == game_client->getClientId())
                    my_player_info = i;
            if (my_player_info && gameGlobalInfo)
            {
#ifdef __EMSCRIPTEN__
                if (browser_info_label)
                    browser_info_label->setText(
                        "Preferred station: " + browserStationLabel() + "\n"
                        "Player state received. Opening ship selection flow."
                    );
                browserDiag("join: player state ready");
#endif
                returnToShipSelection(getRenderLayer());
                destroy();
            }
        }
        break;
    }
}
