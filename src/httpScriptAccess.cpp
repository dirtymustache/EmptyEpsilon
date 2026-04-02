#include "httpScriptAccess.h"
#include "gameGlobalInfo.h"
#include "playerInfo.h"
#include "scenarioInfo.h"
#include "script.h"
#include "crewPosition.h"
#include "multiplayer_server.h"
#include "engine.h"
#include "preferenceManager.h"
#include <filesystem>
#include <fstream>

#define sOBJECT "_OBJECT_"

namespace {
string readTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return "";
    std::string content{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    return string(content);
}

std::filesystem::path getBrowserBundleRoot()
{
    return std::filesystem::path(PreferencesManager::get("browser_bundle_root", "build-wasm/asset_bundles").c_str());
}

std::filesystem::path getScenarioManifestPath(const string& scenario_filename)
{
    return getBrowserBundleRoot() / "scenarios" / (scenario_filename + ".json");
}

std::filesystem::path getBundleArtifactPath(const string& bundle_filename)
{
    return getBrowserBundleRoot() / "bundles" / std::filesystem::path(bundle_filename.c_str());
}

void addBrowserBundleHandlers(sp::io::http::Server& server)
{
    std::error_code error;
    const auto bundle_root = getBrowserBundleRoot() / "bundles";
    if (!std::filesystem::exists(bundle_root, error))
        return;
    for (const auto& entry : std::filesystem::directory_iterator(bundle_root, error))
    {
        if (error || !entry.is_regular_file())
            continue;
        const auto filename = string(entry.path().filename().u8string());
        server.addURLHandler("/browser/bundles/" + filename, [filename](const sp::io::http::Server::Request&) -> string
        {
            auto payload = readTextFile(getBundleArtifactPath(filename));
            if (payload.empty())
                return "{\"ERROR\":\"Bundle not found\"}";
            return payload;
        });
    }
}

string jsonEscape(const string& value)
{
    string escaped;
    for (auto ch : value)
    {
        switch(ch)
        {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

string crewPositionsToJson(const std::vector<CrewPositions>& monitors)
{
    string result = "[";
    bool first_monitor = true;
    for (const auto& monitor : monitors)
    {
        if (!first_monitor)
            result += ",";
        first_monitor = false;
        result += "[";
        bool first_position = true;
        auto monitor_copy = monitor;
        for (auto position : monitor_copy)
        {
            if (!first_position)
                result += ",";
            first_position = false;
            result += "\"" + jsonEscape(crewPositionToString(position)) + "\"";
        }
        result += "]";
    }
    result += "]";
    return result;
}

string buildBrowserSessionManifestJson()
{
    const auto scenario_manifest_path = getScenarioManifestPath(gameGlobalInfo ? gameGlobalInfo->browser_asset_scenario_file : "");
    const auto scenario_manifest_body = readTextFile(scenario_manifest_path);
    string json = "{";
    json += "\"schema_version\":1,";
    json += "\"session_id\":\"" + jsonEscape(gameGlobalInfo ? gameGlobalInfo->browser_asset_session_id : "") + "\",";
    json += "\"scenario_file\":\"" + jsonEscape(gameGlobalInfo ? gameGlobalInfo->browser_asset_scenario_file : "") + "\",";
    json += "\"manifest_revision\":\"" + jsonEscape(gameGlobalInfo ? gameGlobalInfo->browser_asset_manifest_revision : "") + "\",";
    json += "\"scenario_manifest_url\":\"/asset_bundles/scenarios/" + jsonEscape(gameGlobalInfo ? gameGlobalInfo->browser_asset_scenario_file : "") + ".json\",";
    json += "\"bundle_root\":\"/asset_bundles/bundles/\",";
    json += "\"fallback_bundle_root\":\"/admin-api/browser/bundles/\",";
    json += "\"scenario_manifest\":";
    if (!scenario_manifest_body.empty())
        json += scenario_manifest_body;
    else
        json += "null";
    json += "}";
    return json;
}

string buildAdminStatusJson()
{
    string json = "{";
    json += "\"server_name\":\"" + jsonEscape(game_server ? game_server->getServerName() : "Server") + "\",";
    json += "\"scenario\":\"" + jsonEscape(gameGlobalInfo ? gameGlobalInfo->scenario : "") + "\",";
    json += "\"mission_time\":\"" + jsonEscape(gameGlobalInfo ? gameGlobalInfo->getMissionTime() : "00:00") + "\",";
    json += "\"elapsed_time\":" + string(gameGlobalInfo ? gameGlobalInfo->elapsed_time : 0.0f, 1) + ",";
    json += "\"paused\":" + string(engine && engine->getGameSpeed() == 0.0f ? "true" : "false") + ",";
    json += "\"game_speed\":" + string(engine ? engine->getGameSpeed() : 0.0f, 2) + ",";
    json += "\"player_count\":" + string(static_cast<unsigned int>(player_info_list.size())) + ",";
    json += "\"players\":[";

    bool first_player = true;
    foreach(PlayerInfo, player, player_info_list)
    {
        if (!first_player)
            json += ",";
        first_player = false;
        json += "{";
        json += "\"client_id\":" + string(player->client_id) + ",";
        json += "\"name\":\"" + jsonEscape(player->name) + "\",";
        json += "\"ship_id\":\"" + jsonEscape(player->ship ? player->ship.toString() : "") + "\",";
        json += "\"positions\":" + crewPositionsToJson(player->crew_positions) + ",";
        json += "\"main_screen\":" + string(player->main_screen) + ",";
        json += "\"main_screen_control\":" + string(player->main_screen_control);
        json += "}";
    }

    json += "]";
    json += "}";
    return json;
}

string buildScenarioListJson()
{
    string json = "{";
    json += "\"categories\":[";

    auto categories = ScenarioInfo::getCategories();
    bool first_category = true;
    for (const auto& category : categories)
    {
        if (!first_category)
            json += ",";
        first_category = false;
        json += "{";
        json += "\"name\":\"" + jsonEscape(category) + "\",";
        json += "\"scenarios\":[";

        bool first_scenario = true;
        for (const auto& info : ScenarioInfo::getScenarios(category))
        {
            if (!first_scenario)
                json += ",";
            first_scenario = false;
            json += "{";
            json += "\"filename\":\"" + jsonEscape(info.filename) + "\",";
            json += "\"name\":\"" + jsonEscape(info.name) + "\"";
            json += "}";
        }

        json += "]";
        json += "}";
    }

    json += "]";
    json += "}";
    return json;
}
}

EEHttpServer::EEHttpServer(int port, string static_file_path)
: server(port)
{
    server.setStaticFilePath(static_file_path);
    addBrowserBundleHandlers(server);
    server.addURLHandler("/exec.lua", [](const sp::io::http::Server::Request& request) -> string
    {
        if (!gameGlobalInfo)
            return "{\"ERROR\": \"No game\"}";

        sp::script::Environment env(gameGlobalInfo->script_environment_base.get());
        setupSubEnvironment(env);
        auto result = env.run<string>(request.post_data);
        string output;
        if (result.isErr()) {
            output = "{\"ERROR\": \"Script error: " + result.error().replace("\"", "'") + "\"}";
        } else {
            output = result.value();
        }
        return output;
    });
    server.addURLHandler("/status.js", [](const sp::io::http::Server::Request&) -> string
    {
        return "window.EmptyEpsilonAdminStatus && window.EmptyEpsilonAdminStatus(" + buildAdminStatusJson() + ");";
    });
    server.addURLHandler("/scenarios.js", [](const sp::io::http::Server::Request&) -> string
    {
        return "window.EmptyEpsilonAdminScenarios && window.EmptyEpsilonAdminScenarios(" + buildScenarioListJson() + ");";
    });
    server.addURLHandler("/browser/session-manifest.json", [](const sp::io::http::Server::Request&) -> string
    {
        if (!gameGlobalInfo)
            return "{\"ERROR\":\"No game\"}";
        return buildBrowserSessionManifestJson();
    });
    server.addURLHandler("/get.lua", [](const sp::io::http::Server::Request& request) -> string
    {
        /*
        Call LUA-exposed functions and return their result in a dictionary.
        Use _OBJECT_=someObjectGetter() to get the object of which to call functions
        Defaults to getPlayerShip(-1)

        Syntax: /get.lua?dictionaryKey=functionName("arguments)

        Example Getter: /get.lua?hull=getHull()&nukes=getWeaponStorage("nuke")
        Creates the following LUA-code:

        object = getPlayerShip(-1)
        if object == nil then return {error = "No valid object"} end
        return {hull = object:getHull(), nukes = object:getWeaponStorage("nuke"), }

        Returns: {hull = 100, nukes = 42}
        */
        if (!gameGlobalInfo)
        {
            return "{\"ERROR\": \"No game\"}";
        }

        /*TODO
        string luaCode;
        string objectId = "getPlayerShip(-1)";
        if (my_spaceship) {
            int index = gameGlobalInfo->findPlayerShip(my_spaceship);
            objectId = "getPlayerShip("+std::to_string(index+1)+")";
        }
        std::unordered_map<string, string>::const_iterator i;
        P<ScriptObject> script;
        string output;

        // Look for _OBJECT_ in parameters. If not found, use default
        i = request.query.find(sOBJECT);
        if (i != request.query.end())
        {
            objectId = i->second;
        }

        luaCode = "object = " + objectId + "\n" +
                  "if object == nil then return {error = \"No valid object\"} end\n" +
                  "return {";

        // Loop through URL parameters
        for (i = request.query.begin(); i != request.query.end(); i++)
        {
            if (i->first == sOBJECT)
                continue;
            // Fail if trying to set stuff. We only do get.
            if (i->second.substr(0, 3) == "set")
            {
                return "{\"ERROR\": \"Cannot set values through get.lua\", \"COMMAND\": \"" + i->second + "\"}";
            }
            // Build LUA-code
            luaCode += i->first + " = object:" + i->second + ", ";
        }   luaCode += "}";

        // Run script
        script = new ScriptObject();
        script->setMaxRunCycles(100000);

        // Return dictionary with error, else output
        if (!script->runCode(luaCode, output))
        {
            output = "{\"ERROR\": \"Script error\"}";
        }
        script->destroy();
        return output;
        */
        return "TODO";
    });
    server.addURLHandler("/set.lua", [](const sp::io::http::Server::Request& request) -> string
    {
        /*
        Call LUA-exposed functions with arguments.
        Use _OBJECT_=someObjectGetter() to get the object of which to call functions
        Defaults to getPlayerShip(-1)

        Syntax: /set.lua?someFunction('arg')&otherFunction(1,2,3)

        Example Setter: /set.lua?setShieldsActive(true)&setSpeed(200, 3)
        Creates the following LUA-code:

        object = getPlayerShip(-1)
        if object == nil then return {error = "No valid object"} end
        object:setShieldsActive(true);
        object:setSpeed(200, 3);

        Returns nothing, or ERROR on failure.
        */
        if (!gameGlobalInfo)
        {
            return "{\"ERROR\": \"No game\"}";
        }
        /*TODO
        string luaCode;
        string objectId = "getPlayerShip(-1)";
        if (my_spaceship) {
            int index = gameGlobalInfo->findPlayerShip(my_spaceship);
            objectId = "getPlayerShip("+std::to_string(index+1)+")";
        }
        std::unordered_map<string, string>::const_iterator i;
        P<ScriptObject> script;
        string output;

        i = request.query.find(sOBJECT);
        if (i != request.query.end())
        {
            objectId = i->first;
        }

        luaCode = "object = " + objectId + "\n" +
               "if object == nil then return {error = \"No valid object\"} end\n";

        for (i = request.query.begin(); i != request.query.end(); i++)
        {
            if (i->first == sOBJECT)
                continue;
            if (i->second == "")
                luaCode += "object:" + i->first + ";\n";
            else
                luaCode += i->first + ":" + i->second + ";\n";
        }

        script = new ScriptObject();
        script->setMaxRunCycles(100000);

        if (!script->runCode(luaCode, output))
            output = "{\"ERROR\": \"Script error\"}";
        else
            output = "{}";
        script->destroy();
        return output;
        */
        return "TODO";
    });
}
