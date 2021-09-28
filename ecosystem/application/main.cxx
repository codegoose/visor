#include "application.h"
#include "gui.h"

#define SC_FEATURE_SENTRY
#define SC_FEATURE_MINIMAL_REDRAW
#define SC_FEATURE_ENHANCED_FONTS
#define SC_FEATURE_RENDER_ON_RESIZE

#define SC_VIEW_INIT_W 800
#define SC_VIEW_INIT_H 600
#define SC_VIEW_MIN_W 640
#define SC_VIEW_MIN_H 480

#include "../boot/imgui_gl3_glfw3.hpp"

static std::optional<std::string> sc::boot::on_startup() {
    visor::gui::initialize();
    return std::nullopt;
}

static tl::expected<bool, std::string> sc::boot::on_fixed_update() {
    return true;
}

static tl::expected<bool, std::string> sc::boot::on_update(const glm::ivec2 &framebuffer_size, bool *const force_redraw) {
    sc::visor::gui::emit(framebuffer_size, force_redraw);
    return visor::keep_running;
}

static void sc::boot::on_shutdown() {
    visor::gui::shutdown();
}