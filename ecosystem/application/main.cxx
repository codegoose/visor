#include "application.h"

#define SC_FEATURE_SENTRY
#define SC_FEATURE_MINIMAL_REDRAW
#define SC_FEATURE_ENHANCED_FONTS
#define SC_FEATURE_SYSTEM_TRAY

#include "../boot/imgui_gl3_sdl2.hpp"
#include "../file/file.h"
#include "../systray/systray.h"
#include "../texture/texture.h"
#include "../imgui/imgui_utils.hpp"

static std::vector<std::shared_ptr<sc::texture::gpu_handle>> uploaded_frames;

static std::optional<std::string> on_startup() {
    if (const auto content = sc::file::load("C:/Users/bwhit/Downloads/75839-jump-through-4-hoops.json"); content) {
        if (const auto sequence = sc::texture::load_lottie_from_memory("test", *content, { 256, 256 }); sequence) {
            for (const auto &frame : sequence->frames) {
                if (const auto texture = sc::texture::upload_to_gpu(sequence->frames.front(), sequence->frames.front().size / 2); texture) {
                    uploaded_frames.push_back(*texture);
                } else spdlog::error(texture.error());
            }
        } else spdlog::error(sequence.error());
    } else spdlog::error(content.error());
    return std::nullopt;
}

static tl::expected<bool, std::string> on_system_event(const SDL_Event &event) {
    if (event.type == SDL_QUIT) return false;
    return true;
}

static tl::expected<bool, std::string> on_fixed_update() {
    return true;
}

static tl::expected<bool, std::string> on_update(const glm::ivec2 &framebuffer_size) {
    sc::visor::emit_ui(framebuffer_size);
    return true;
}

static void on_shutdown() {
    uploaded_frames.clear();
}