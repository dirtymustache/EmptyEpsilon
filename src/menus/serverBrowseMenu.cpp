#include <i18n.h>
#include "main.h"
#include "serverBrowseMenu.h"
#include "joinServerMenu.h"
#include "multiplayer_server_scanner.h"
#include "preferenceManager.h"
#include "config.h"

#include "gui/theme.h"
#include "gui/gui2_overlay.h"
#include "gui/gui2_button.h"
#include "gui/gui2_selector.h"
#include "gui/gui2_textentry.h"
#include "gui/gui2_label.h"
#include "gui/gui2_listbox.h"

namespace
{
    const std::vector<std::pair<string, string>>& browserStationOptions()
    {
        static const std::vector<std::pair<string, string>> options{
            { "Relay", "relay" },
            { "Science", "science" },
            { "Helms", "helms" },
            { "Weapons", "weapons" },
            { "Engineering", "engineering" },
            { "Operations", "operations" },
            { "Tactical", "tactical" },
            { "Single Pilot", "singlepilot" },
            { "Main Screen", "mainscreen" },
        };
        return options;
    }

    const string disconnectErrorMessage(GameClient::DisconnectReason reason)
    {
        switch (reason)
        {
        case GameClient::DisconnectReason::None:
            return tr("game_client_disconnect_reason", "still connected");
        case GameClient::DisconnectReason::FailedToConnect:
            return tr("game_client_disconnect_reason", "failed to connect to server");
        case GameClient::DisconnectReason::BadCredentials:
            return tr("game_client_disconnect_reason", "bad credentials");
        case GameClient::DisconnectReason::ClosedByServer:
            return tr("game_client_disconnect_reason", "closed by server");
        case GameClient::DisconnectReason::TimedOut:
            return tr("game_client_disconnect_reason", "timed out");
        case GameClient::DisconnectReason::Unknown:
            return tr("game_client_disconnect_reason", "unknown");
        case GameClient::DisconnectReason::VersionMismatch:
            return tr("game_client_disconnect_reason", "version mismatch");
        default:
            return tr("game_client_disconnect_reason", "unspecified error {error}").format({ {"error", string{static_cast<int>(reason)}} });
        }
    }
}

ServerBrowserMenu::ServerBrowserMenu(std::optional<GameClient::DisconnectReason> last_attempt /* = {} */)
{
#ifdef __EMSCRIPTEN__
    new GuiOverlay(this, "", GuiTheme::getColor("background"));
    (new GuiOverlay(this, "", glm::u8vec4{255,255,255,255}))->setTextureTiledThemed("background.crosses");

    (new GuiButton(this, "BACK", tr("button", "Back"), [this]() {
        destroy();
        returnToMainMenu(getRenderLayer());
    }))->setPosition(50, -50, sp::Alignment::BottomLeft)->setSize(300, 50);

    (new GuiLabel(this, "BROWSER_NET_TITLE", tr("mainMenu", "Connect via WebSocket bridge"), 32))
        ->setPosition(0, 80, sp::Alignment::TopCenter)
        ->setSize(0, 40);

    if (last_attempt)
    {
        auto error_message = tr("Connection error: {message}").format({ {"message", disconnectErrorMessage(*last_attempt)} });
        auto error_info = new GuiLabel(this, "LAST_ATTEMPT_ERROR_MESSAGE", error_message, 26);
        error_info->setPosition(0, 120, sp::Alignment::TopCenter);
    }

    string bridge_note =
        "Enter a websocket bridge URL below.\n\n"
        "Expected direction:\n"
        "browser client <-> WebSocket bridge <-> native EmptyEpsilon server\n\n"
        "The bridge path is now usable for browser multiplayer smoke tests,\n"
        "but LAN browsing and raw native sockets are still desktop-only.";

    auto info = new GuiLabel(this, "BROWSER_NET_INFO", bridge_note, 24);
    info->setPosition(0, 170, sp::Alignment::TopCenter)->setSize(900, 180);

    (new GuiLabel(this, "BROWSER_NET_STATION_LABEL", tr("station", "Preferred station"), 26))
        ->setPosition(0, 370, sp::Alignment::TopCenter)
        ->setSize(0, 30);

    browser_station_selector = new GuiSelector(this, "BROWSER_STATION_SELECTOR", [](int, string value) {
        PreferencesManager::set("browser_station", value);
    });
    for (const auto& [label, value] : browserStationOptions())
        browser_station_selector->addEntry(label, value);
    browser_station_selector->setPosition(0, 410, sp::Alignment::TopCenter)->setSize(420, 50);

    auto preferred_station = PreferencesManager::get("browser_station", "relay");
    int preferred_station_index = 0;
    for (int n = 0; n < int(browserStationOptions().size()); n++)
        if (browserStationOptions()[n].second == preferred_station)
            preferred_station_index = n;
    browser_station_selector->setSelectionIndex(preferred_station_index);

    connect_button = new GuiButton(this, "CONNECT", tr("screenLan", "Connect"), [this]() {
        connect(manual_ip->getText());
    });
    connect_button->setPosition(-50, -50, sp::Alignment::BottomRight)->setSize(300, 50);

    manual_ip = new GuiTextEntry(this, "BRIDGE_URL", PreferencesManager::get("browser_bridge_url", "ws://127.0.0.1:35667"));
    manual_ip->setPosition(0, 490, sp::Alignment::TopCenter)->setSize(700, 50);
    manual_ip->enterCallback([this](string text) {
        connect(text);
    });
    return;
#endif

    scanner = new ServerScanner(VERSION_NUMBER);
    scanner->scanLocalNetwork();
    scanner->scanMasterServer(PreferencesManager::get("registry_list_url", "http://daid.eu/ee/list.php"));

    new GuiOverlay(this, "", GuiTheme::getColor("background"));
    (new GuiOverlay(this, "", glm::u8vec4{255,255,255,255}))->setTextureTiledThemed("background.crosses");

    (new GuiButton(this, "BACK", tr("button", "Back"), [this]() {
        destroy();
        returnToMainMenu(getRenderLayer());
    }))->setPosition(50, -50, sp::Alignment::BottomLeft)->setSize(300, 50);

    if (last_attempt)
    {
        auto error_message = tr("Connection error: {message}").format({ {"message", disconnectErrorMessage(*last_attempt)} });
        auto error_info = new GuiLabel(this, "LAST_ATTEMPT_ERROR_MESSAGE", error_message, 30);
        error_info->setPosition(0, 25, sp::Alignment::TopCenter);
    }

    connect_button = new GuiButton(this, "CONNECT", tr("screenLan", "Connect"), [this]() {
        if (selected_server) {
            connect(selected_server.value());
        } else {
            connect(manual_ip->getText());
        }
    });
    connect_button->setPosition(-50, -50, sp::Alignment::BottomRight)->setSize(300, 50);

    manual_ip = new GuiTextEntry(this, "IP", "");
    manual_ip->setPosition(-50, -120, sp::Alignment::BottomRight)->setSize(300, 50);
    manual_ip->callback([this](string text) {
        selected_server.reset();
    });
    manual_ip->enterCallback([this](string text) {
        connect(text);
    });
    server_list_box = new GuiListbox(this, "SERVERS", [this](int index, string value) {
        if (value == "last_server") {
            manual_ip->setText(PreferencesManager::get("last_server", ""));
            selected_server.reset();
        } else {
            selected_server = server_list[value.toInt()];
            manual_ip->setText(selected_server.value().address.getHumanReadable()[0]);
        }
    });
    scanner->addCallbacks([this](const ServerScanner::ServerInfo& info) {
        //New server found
        if (info.address.getHumanReadable().empty()) return;
        server_list.push_back(info);
        updateServerList();
        if (manual_ip->getText() == "")
            manual_ip->setText(info.address.getHumanReadable()[0]);
    }, [this](const ServerScanner::ServerInfo& info) {
        //Server removed from list
        if (info.address.getHumanReadable().empty()) return;
        server_list.erase(std::remove_if(server_list.begin(), server_list.end(), [&info](const ServerScanner::ServerInfo& entry){
            return info.type == entry.type && info.address == entry.address && info.port == entry.port;
        }), server_list.end());
    });
    server_list_box->setPosition(0, 50, sp::Alignment::TopCenter)->setSize(700, 600);
    updateServerList();
}

void ServerBrowserMenu::updateServerList()
{
    server_list_box->setOptions({});
    if (PreferencesManager::get("last_server", "") != "") {
        server_list_box->addEntry(tr("Last Session ({last})").format({{"last", PreferencesManager::get("last_server", "")}}), "last_server");
    }
    std::stable_sort(server_list.begin(), server_list.end(), [](const auto& a, const auto& b) {
        //Sort by type, then by server name, and finally by IP address (prefering short addresses first)
        if (a.type == b.type && a.name == b.name) {
            auto aa = a.address.getHumanReadable()[0];
            auto ba = b.address.getHumanReadable()[0];
            if (aa.size() == ba.size())
                return aa < ba;
            return aa.size() < ba.size();
        }
        if (a.type == b.type)
            return a.name < b.name;
        return a.type < b.type;
    });
    for(int idx = 0; idx < int(server_list.size()); idx++) {
        const auto& entry = server_list[idx];
        auto label = entry.name + " (" + entry.address.getHumanReadable()[0] + ")";
        switch(entry.type) {
        case ServerScanner::ServerType::Manual:
            break;
        case ServerScanner::ServerType::LAN:
            label = "LAN: " + label;
            break;
        case ServerScanner::ServerType::MasterServer:
            label = "Internet: " + label;
            break;
        case ServerScanner::ServerType::SteamFriend:
            label = "Steam: " + entry.name;
            break;
        }
        server_list_box->addEntry(label, string(idx));
    }
}


ServerBrowserMenu::~ServerBrowserMenu()
{
#ifndef __EMSCRIPTEN__
    scanner->destroy();
#endif
}

void ServerBrowserMenu::connect(string host)
{
#ifdef __EMSCRIPTEN__
    host = host.strip();
    if (!host.startswith("ws://") && !host.startswith("wss://"))
        host = "ws://" + host;
    PreferencesManager::set("browser_bridge_url", host);
    ServerScanner::ServerInfo info;
    info.type = ServerScanner::ServerType::Manual;
    info.name = host;
    connect(info);
    return;
#else
    host = host.strip();
    uint64_t port = defaultServerPort;
    if (host.find(":") != -1)
    {
        port = host.substr(host.find(":") + 1).toInt64();
        host = host.substr(0, host.find(":"));
    }
    ServerScanner::ServerInfo info;
    info.type = ServerScanner::ServerType::Manual;
    info.name = host;
    info.port = port;
    info.address = sp::io::network::Address(host);
    connect(info);
#endif
}

void ServerBrowserMenu::connect(const ServerScanner::ServerInfo& info)
{
#ifdef __EMSCRIPTEN__
    new JoinServerScreen(info);
    destroy();
    return;
#endif
    new JoinServerScreen(info);
    destroy();
}
