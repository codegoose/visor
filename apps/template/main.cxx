#include "main.rc"

#define SC_FEATURE_MINIMAL_REDRAW
#define SC_FEATURE_ENHANCED_FONTS
#define SC_FEATURE_RENDER_ON_RESIZE
#define SC_FEATURE_CENTER_WINDOW

#define SC_VIEW_INIT_W 300
#define SC_VIEW_INIT_H 500
#define SC_VIEW_MIN_W SC_VIEW_INIT_W
#define SC_VIEW_MIN_H SC_VIEW_INIT_H

#include "../../libs/boot/imgui_gl3_glfw3.hpp"

#include <iostream>

static std::optional<std::string> sc::boot::on_startup() {
    auto &style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Tab] = { 50.f / 255.f, 50.f / 255.f, 50.f / 255.f, 1.f };
    style.Colors[ImGuiCol_TabActive] = { 70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.f };
    style.Colors[ImGuiCol_TabHovered] = { 90.f / 255.f, 90.f / 255.f, 90.f / 255.f, 1.f };
    style.Colors[ImGuiCol_WindowBg] = { 0x21 / 255.f, 0x25 / 255.f, 0x29 / 255.f, 1.f };
    style.Colors[ImGuiCol_ChildBg] = { 50.f / 255.f, 50.f / 255.f, 50.f / 255.f, 1.f };
    style.Colors[ImGuiCol_FrameBgActive] = { 70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.f };
    style.Colors[ImGuiCol_FrameBgHovered] = { 90.f / 255.f, 90.f / 255.f, 90.f / 255.f, 1.f };
    style.Colors[ImGuiCol_FrameBg] = { 42.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f };
    style.Colors[ImGuiCol_ModalWindowDimBg] = { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .88f };
    style.WindowBorderSize = 1;
    style.FrameBorderSize = 1;
    style.FrameRounding = 3.f;
    style.ChildRounding = 3.f;
    style.ScrollbarRounding = 3.f;
    style.WindowRounding = 3.f;
    style.GrabRounding = 3.f;
    style.TabRounding = 3.f;
    style.Colors[ImGuiCol_ChildBg] = { .09f, .09f, .09f, 1.f };
    return std::nullopt;
}

static tl::expected<bool, std::string> sc::boot::on_fixed_update() {
    return true;
}

static tl::expected<bool, std::string> sc::boot::on_update(const glm::ivec2 &framebuffer_size, bool *const force_redraw) {
    return true;
}

static void sc::boot::on_shutdown() {

}
