#pragma once

#include <stringImproved.h>

string initConfiguration(int argc, char** argv);
void saveConfiguration(const string& configuration_path);

#ifdef __EMSCRIPTEN__
extern "C" void ee_browser_save_configuration();
extern "C" void ee_browser_play_test_sound();
#endif

#ifdef __EMSCRIPTEN__
extern "C" void ee_browser_save_configuration();
#endif
