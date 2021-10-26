#include "application.h"
#include "gui.h"

#define SC_FEATURE_SENTRY
#define SC_FEATURE_MINIMAL_REDRAW
#define SC_FEATURE_ENHANCED_FONTS
#define SC_FEATURE_RENDER_ON_RESIZE
#define SC_FEATURE_SYSTEM_TRAY
#define SC_FEATURE_CENTER_WINDOW

#define SC_VIEW_INIT_W 728
#define SC_VIEW_INIT_H 636
#define SC_VIEW_MIN_W SC_VIEW_INIT_W
#define SC_VIEW_MIN_H SC_VIEW_INIT_H

#include "../boot/imgui_gl3_glfw3.hpp"
#include "../api/api.h"
#include "../iracing/iracing.h"

#include "legacy.h"

static std::optional<std::string> sc::boot::on_startup() {
    iracing::startup();
    visor::gui::initialize();
    if (const auto res = sc::api::customer::get_session_token("miranda@google.com", "abc123").get(); res) {
        spdlog::critical("Session key: {}", res->dump());
    } else spdlog::error(res.error());
    return std::nullopt;
}

static tl::expected<bool, std::string> sc::boot::on_fixed_update() {
    return true;
}

static tl::expected<bool, std::string> sc::boot::on_update(const glm::ivec2 &framebuffer_size, bool *const force_redraw) {
    visor::gui::emit(framebuffer_size, force_redraw);
    visor::legacy::process();
    return visor::keep_running;
}

static void sc::boot::on_shutdown() {
    visor::gui::shutdown();
    iracing::shutdown();
    visor::legacy::disable();
}