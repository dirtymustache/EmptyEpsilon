#ifndef SERVER_CREATION_SCREEN_H
#define SERVER_CREATION_SCREEN_H

#include "gui/gui2_canvas.h"
#include "Updatable.h"

class GuiScrollText;
class GuiSelector;
class GuiTextEntry;
class GuiListbox;
class GuiButton;
class GuiLabel;


class ServerSetupScreen : public GuiCanvas
{
public:
    ServerSetupScreen();

private:
    GuiTextEntry* server_name;
    GuiTextEntry* server_password;
    GuiTextEntry* gm_password;
    GuiSelector* server_visibility;
    GuiTextEntry* server_port;
};

class ServerSetupMasterServerRegistrationScreen : public GuiCanvas, Updatable
{
public:
    ServerSetupMasterServerRegistrationScreen();

    virtual void update(float delta) override;

private:
    GuiLabel* info_label;
    GuiButton* continue_button;
};

class ServerScenarioSelectionScreen : public GuiCanvas, Updatable
{
public:
    ServerScenarioSelectionScreen();
    virtual void update(float delta) override;

private:
    void loadScenarioList(const string& category);
    void startScenarioNow(const string& filename);
    GuiListbox* category_list;
    GuiListbox* scenario_list;
    GuiScrollText* description_text;
    GuiButton* start_button;
    string pending_browser_scenario;
    float pending_browser_asset_seconds = 0.0f;
};

class ServerScenarioOptionsScreen : public GuiCanvas, Updatable
{
public:
    ServerScenarioOptionsScreen(string filename);
    virtual void update(float delta) override;

private:
    void startScenarioWhenReady();
    GuiButton* start_button;
    GuiLabel* browser_status_label;
    std::unordered_map<string,string> scenario_settings;
    std::unordered_map<string, GuiScrollText*> description_per_setting;
    string scenario_filename;
    string scenario_name;
    bool waiting_for_browser_assets = false;
    float pending_browser_asset_seconds = 0.0f;
};

#endif//SERVER_CREATION_SCREEN_H
