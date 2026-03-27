#pragma once

#include <stringImproved.h>

string initConfiguration(int argc, char** argv);
void saveConfiguration(const string& configuration_path);

#ifdef __EMSCRIPTEN__
extern "C" void ee_browser_save_configuration();
extern "C" int ee_browser_ensure_audio_started();
extern "C" void ee_browser_play_test_sound();
extern "C" void ee_browser_set_escape_key(int down);
extern "C" void ee_browser_set_voice_key(int target_identifier, int down);
#endif

#ifdef __EMSCRIPTEN__
extern "C" void ee_browser_save_configuration();
#endif
