#include "displaywindows.h"
#include "main.h"
#include "menus/luaConsole.h"
#include <preferenceManager.h>
#include "windowManager.h"
#include "gui/mouseRenderer.h"
#include "graphics/opengl.h"
#include "menus/shipSelectionScreen.h"
#include "shaderRegistry.h"
#include "gui/debugRenderer.h"
#include "glObjects.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
static void browserDiag(const string& message)
{
    EM_ASM({
        if (typeof window.EmptyEpsilonDiag === "function")
            window.EmptyEpsilonDiag(UTF8ToString($0));
    }, message.c_str());
}

EM_JS(float, browserTouchUiScale, (), {
    const coarsePointer = !!(window.matchMedia && window.matchMedia("(pointer: coarse)").matches);
    const touchCapable = coarsePointer || (navigator.maxTouchPoints || 0) > 0;
    if (!touchCapable) {
        return 1.0;
    }

    const shortestSide = Math.min(window.innerWidth || 0, window.innerHeight || 0);
    if (shortestSide > 0 && shortestSide <= 430) {
        return 0.40;
    }
    if (shortestSide > 0 && shortestSide <= 820) {
        return 0.62;
    }
    return 0.82;
});
#endif

bool createDisplayWindows()
{
    //Setup the rendering layers.
    defaultRenderLayer = new RenderLayer();
    consoleRenderLayer = new RenderLayer(defaultRenderLayer);
    mouseLayer = new RenderLayer(consoleRenderLayer);
    glitchPostProcessor = new PostProcessor("shaders/glitch", mouseLayer);
    glitchPostProcessor->enabled = false;
    warpPostProcessor = new PostProcessor("shaders/warp", glitchPostProcessor);
    warpPostProcessor->enabled = false;

    new LuaConsole();

    int width = 1200;
    int height = 900;
    int fsaa = 0;
    Window::Mode fullscreen = (Window::Mode)PreferencesManager::get("fullscreen", "1").toInt();

    if (PreferencesManager::get("fsaa").toInt() > 0)
    {
        fsaa = PreferencesManager::get("fsaa").toInt();
        if (fsaa < 2)
            fsaa = 2;
    }

#ifndef ANDROID
    if (PreferencesManager::get("touchscreen").toInt() == 0)
    {
        engine->registerObject("mouseRenderer", new MouseRenderer(mouseLayer));
        P<MouseRenderer> mouse_renderer = engine->getObject("mouseRenderer");
    }
#endif

#ifdef __EMSCRIPTEN__
    // Browser fullscreen should be handled by the shell page rather than SDL's
    // desktop-style fullscreen modes, which can distort the in-page canvas.
    fullscreen = Window::Mode::Window;
    const float touch_ui_scale = browserTouchUiScale();
    if (touch_ui_scale > 0.0f && touch_ui_scale < 0.999f)
    {
        width = std::max(480, int(width * touch_ui_scale));
        height = std::max(360, int(height * touch_ui_scale));
        browserDiag("display: touch ui scale active");
    }
#endif
    windows.push_back(new Window({width, height}, fullscreen, warpPostProcessor, fsaa));
    window_render_layers.push_back(defaultRenderLayer);

#ifdef __EMSCRIPTEN__
    browserDiag("display: primary window created");
#endif

    if (PreferencesManager::get("multimonitor", "0").toInt() != 0)
    {
        auto n = PreferencesManager::get("multimonitor", "0").toInt();
        if (n < 2)
            n = SDL_GetNumVideoDisplays();
        while(int(windows.size()) < n)
        {
            auto wrl = new RenderLayer();
            auto ml = new RenderLayer(wrl);
            new MouseRenderer(ml);
            windows.push_back(new Window({width, height}, fullscreen, ml, fsaa));
            window_render_layers.push_back(wrl);
            new SecondMonitorScreen(windows.size() - 1);
        }
    }

#if defined(DEBUG)
    // Synchronous gl debug output always in debug.
    constexpr bool wants_gl_debug = true;
    constexpr bool wants_gl_debug_synchronous = true;
#else
    auto wants_gl_debug = !PreferencesManager::get("gl_debug").empty();
    auto wants_gl_debug_synchronous = !PreferencesManager::get("gl_debug_synchronous").empty();
#endif
    if (wants_gl_debug)
    {
        if (sp::gl::enableDebugOutput(wants_gl_debug_synchronous))
            LOG(INFO, "GL Debug output enabled.");
        else
            LOG(WARNING, "GL Debug output requested but not available on this system.");
    }

    for(size_t n=0; n<windows.size(); n++)
    {
        P<Window> window = windows[n];
        string postfix = "";
        if (n > 0)
            postfix = " - " + string(int(n));
        if (PreferencesManager::get("instance_name") != "")
            window->setTitle("EmptyEpsilon - " + PreferencesManager::get("instance_name") + postfix);
        else
            window->setTitle("EmptyEpsilon" + postfix);
        window->setIcon("logo_icon.png");
    }

    if (gl::isAvailable())
    {
#ifdef __EMSCRIPTEN__
        browserDiag("display: GL available, initializing shaders");
#endif
        if (!ShaderRegistry::Shader::initialize())
        {
            LOG(ERROR, "Failed to initialize shaders, exiting.");
#ifdef __EMSCRIPTEN__
            browserDiag("display: shader initialization failed");
#endif
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to initialize shaders (possible cause: cannot find shader files)", nullptr);
            return false;
        }
#ifdef __EMSCRIPTEN__
        browserDiag("display: shaders initialized");
#endif
    }

    new DebugRenderer(mouseLayer);
#ifdef __EMSCRIPTEN__
    browserDiag("display: debug renderer ready");
#endif
    return true;
}
