#include "resources.h"
#include <packResourceProvider.h>
#include <preferenceManager.h>


void initResourcePaths()
{
    if (PreferencesManager::get("mod") != "")
    {
        string mod = PreferencesManager::get("mod");
#if !defined(__EMSCRIPTEN__)
        if (getenv("HOME"))
        {
            new DirectoryResourceProvider(string(getenv("HOME")) + "/.emptyepsilon/resources/mods/" + mod);
            PackResourceProvider::addPackResourcesForDirectory(string(getenv("HOME")) + "/.emptyepsilon/resources/mods/" + mod);
        }
#endif
        new DirectoryResourceProvider("resources/mods/" + mod);
        PackResourceProvider::addPackResourcesForDirectory("resources/mods/" + mod);
    }

    new DirectoryResourceProvider("resources/");
    new DirectoryResourceProvider("scripts/");
    PackResourceProvider::addPackResourcesForDirectory("packs/");
#if !defined(__EMSCRIPTEN__)
    if (getenv("HOME"))
    {
        new DirectoryResourceProvider(string(getenv("HOME")) + "/.emptyepsilon/resources/");
        new DirectoryResourceProvider(string(getenv("HOME")) + "/.emptyepsilon/scripts/");
        PackResourceProvider::addPackResourcesForDirectory(string(getenv("HOME")) + "/.emptyepsilon/packs/");
    }
#endif
#ifdef RESOURCE_BASE_DIR
    new DirectoryResourceProvider(RESOURCE_BASE_DIR "resources/");
    new DirectoryResourceProvider(RESOURCE_BASE_DIR "scripts/");
    PackResourceProvider::addPackResourcesForDirectory(RESOURCE_BASE_DIR "packs");
#endif
}
