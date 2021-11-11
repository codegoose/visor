#include "application.h"
#include "gui.h"

// #define SC_FEATURE_SENTRY
#define SC_FEATURE_MINIMAL_REDRAW
#define SC_FEATURE_ENHANCED_FONTS
#define SC_FEATURE_RENDER_ON_RESIZE
#define SC_FEATURE_SYSTEM_TRAY
#define SC_FEATURE_CENTER_WINDOW

#define SC_VIEW_INIT_W 728
#define SC_VIEW_INIT_H 636
#define SC_VIEW_MIN_W SC_VIEW_INIT_W
#define SC_VIEW_MIN_H SC_VIEW_INIT_H

#include "../../libs/boot/imgui_gl3_glfw3.hpp"
#include "../../libs/iracing/iracing.h"

#include "legacy.h"

static bool enforce_one_instance() {
    const auto mutex = CreateMutex(NULL, TRUE, "SimCoachesVisorEcosystemApplication");
    if (auto mutex_wait_res = WaitForSingleObject(mutex, 0); mutex_wait_res != WAIT_OBJECT_0) {
        MessageBox(NULL, "It seems like Visor is already running. Look for the icon on your taskbar.", "Visor", MB_OK | MB_ICONINFORMATION);
        return false; 
    } else return true;
}

static std::optional<std::string> sc::boot::on_startup() {
    if (!enforce_one_instance()) return "Duplicate instance.";
    // iracing::startup();
    visor::gui::initialize();
    return std::nullopt;
}

static tl::expected<bool, std::string> sc::boot::on_fixed_update() {
    return true;
}

static tl::expected<bool, std::string> sc::boot::on_update(const glm::ivec2 &framebuffer_size, bool *const force_redraw) {
    visor::gui::emit(framebuffer_size, force_redraw);
    if (const auto err = visor::legacy::process(); err) {
        sc::visor::legacy_support_error = true;
        sc::visor::legacy_support_error_description = *err;
    }
    return visor::keep_running;
}

static void sc::boot::on_shutdown() {
    visor::gui::shutdown();
    // iracing::shutdown();
    visor::legacy::disable();
}