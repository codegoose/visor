#include "main.rc" // Include the main resource file

#define SC_FEATURE_MINIMAL_REDRAW // Enable the minimal redraw feature
#define SC_FEATURE_ENHANCED_FONTS // Enable the enhanced fonts feature
#define SC_FEATURE_RENDER_ON_RESIZE // Enable rendering on resize feature
#define SC_FEATURE_CENTER_WINDOW // Enable centering the window feature

#define SC_VIEW_INIT_W 300 // Set the initial width of the view to 300
#define SC_VIEW_INIT_H 500 // Set the initial height of the view to 500
#define SC_VIEW_MIN_W SC_VIEW_INIT_W // Set the minimum width of the view to the initial width
#define SC_VIEW_MIN_H SC_VIEW_INIT_H // Set the minimum height of the view to the initial height

#include "../../libs/boot/imgui_gl3_glfw3.hpp" // Include the imgui_gl3_glfw3 library

#include <iostream> // Include the iostream library

static std::optional<std::string> sc::boot::on_startup() {
    auto &style = ImGui::GetStyle(); // Get a reference to the ImGui style
    style.Colors[ImGuiCol_Tab] = { 50.f / 255.f, 50.f / 255.f, 50.f / 255.f, 1.f }; // Set the color for ImGuiCol_Tab
    style.Colors[ImGuiCol_TabActive] = { 70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.f }; // Set the color for ImGuiCol_TabActive
    style.Colors[ImGuiCol_TabHovered] = { 90.f / 255.f, 90.f / 255.f, 90.f / 255.f, 1.f }; // Set the color for ImGuiCol_TabHovered
    style.Colors[ImGuiCol_WindowBg] = { 0x21 / 255.f, 0x25 / 255.f, 0x29 / 255.f, 1.f }; // Set the color for ImGuiCol_WindowBg
    style.Colors[ImGuiCol_ChildBg] = { 50.f / 255.f, 50.f / 255.f, 50.f / 255.f, 1.f }; // Set the color for ImGuiCol_ChildBg
    style.Colors[ImGuiCol_FrameBgActive] = { 70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.f }; // Set the color for ImGuiCol_FrameBgActive
    style.Colors[ImGuiCol_FrameBgHovered] = { 90.f / 255.f, 90.f / 255.f, 90.f / 255.f, 1.f }; // Set the color for ImGuiCol_FrameBgHovered
    style.Colors[ImGuiCol_FrameBg] = { 42.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f }; // Set the color for ImGuiCol_FrameBg
    style.Colors[ImGuiCol_ModalWindowDimBg] = { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .88f }; // Set the color for ImGuiCol_ModalWindowDimBg
    style.WindowBorderSize = 1; // Set the border size for windows
    style.FrameBorderSize = 1; // Set the border size for frames
    style.FrameRounding = 3.f; // Set the rounding for frames
    style.ChildRounding = 3.f; // Set the rounding for child windows
    style.ScrollbarRounding = 3.f; // Set the rounding for scrollbars
    style.WindowRounding = 3.f; // Set the rounding for windows
    style.GrabRounding = 3.f; // Set the rounding for grab areas
    style.TabRounding = 3.f; // Set the rounding for tabs
    style.Colors[ImGuiCol_ChildBg] = { .09f, .09f, .09f, 1.f }; // Set the color for ImGuiCol_ChildBg
    return std::nullopt; // Return an empty optional
}

static tl::expected<bool, std::string> sc::boot::on_fixed_update() {
    return true; // Return a successful expected value with true
}

static tl::expected<bool, std::string> sc::boot::on_update(const glm::ivec2 &framebuffer_size, bool *const force_redraw) {
    return true; // Return a successful expected value with true
}

static void sc::boot::on_shutdown() {
    // Empty function
}
