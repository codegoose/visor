#include "application.h"
#include "discovery.h"

#define SC_FEATURE_SENTRY
#define SC_FEATURE_MINIMAL_REDRAW
#define SC_FEATURE_ENHANCED_FONTS

#include "../boot/imgui_gl3_sdl2.hpp"
#include "../resource/resource.h"
#include "../texture/texture.h"
#include "../imgui/imgui_utils.hpp"

static std::vector<std::shared_ptr<sc::texture::gpu_handle>> uploaded_frames;

static std::optional<std::string> imgui_prepare_styling() {
    auto &style = ImGui::GetStyle();
    style.WindowBorderSize = 1;
    style.FrameBorderSize = 1;
    style.FrameRounding = 3.f;
    style.ChildRounding = 3.f;
    style.ScrollbarRounding = 3.f;
    style.WindowRounding = 3.f;
    style.GrabRounding = 3.f;
    return std::nullopt;
}

static std::optional<std::string> sc::boot::on_startup() {
    imgui_prepare_styling();
    const auto resource_name = "LOTTIE_NOT_FOUND_CONE";
    if (const auto content = sc::resource::get_resource("DATA", resource_name); content) {
        std::vector<std::byte> buffer(content->second);
        memcpy(buffer.data(), content->first, buffer.size());
        if (const auto sequence = sc::texture::load_lottie_from_memory(resource_name, buffer, { 256, 256 }); sequence) {
            int frame_i = 0;
            for (const auto &frame : sequence->frames) {
                const auto description = pystring::lower(fmt::format("<rsc:{}:{}x{}#{}>", resource_name, sequence->frames.front().size.x, sequence->frames.front().size.y, frame_i++));
                if (const auto texture = sc::texture::upload_to_gpu(sequence->frames.front(), sequence->frames.front().size, description); texture) {
                    uploaded_frames.push_back(*texture);
                } else spdlog::error(texture.error());
            }
        }
    }
    visor::discovery::startup();
    return std::nullopt;
}

static tl::expected<bool, std::string> sc::boot::on_system_event(const SDL_Event &event) {
    if (event.type == SDL_QUIT) return false;
    return true;
}

static tl::expected<bool, std::string> sc::boot::on_fixed_update() {
    return true;
}

static tl::expected<bool, std::string> sc::boot::on_update(const glm::ivec2 &framebuffer_size) {
    sc::visor::emit_ui(framebuffer_size);
    return visor::keep_running;
}

static void sc::boot::on_shutdown() {
    visor::discovery::shutdown();
    uploaded_frames.clear();
}