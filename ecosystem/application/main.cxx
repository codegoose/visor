#include <optional>
#include <vector>
#include <string_view>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <glbinding/glbinding.h>
#include <glbinding/gl33core/gl.h>
#include <imgui.h>

#include "application.h"

#include "../imgui/imgui_impl_sdl.h"
#include "../imgui/imgui_impl_opengl3.h"
#include "../imgui/imgui_freetype.h"
#include "../imgui/imgui_utils.hpp"
#include "../defer.h"
#include "../font/font_awesome_5.h"
#include "../font/font_awesome_5_brands.h"
#include "../hidhide/hidhide.h"

using namespace gl;

static std::function<glbinding::ProcAddress(const char*)> sc_get_proc_address = [](const char *name) -> glbinding::ProcAddress {
    const auto addr = SDL_GL_GetProcAddress(name);
    const auto text = fmt::format("GL: {} @ {}", name, reinterpret_cast<void *>(addr));
    if (addr) spdlog::debug(text);
    else {
        spdlog::error(text);
        exit(1000);
    }
    return reinterpret_cast<glbinding::ProcAddress>(addr);
};

static const bool prepare_styling() {
    if (!ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 16)) {
        spdlog::critical("Unable to load primary font.");
        return false;
    }
    {
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        if (!ImGui::GetIO().Fonts->AddFontFromFileTTF( FONT_ICON_FILE_NAME_FAS, 12, &icons_config, icons_ranges)) {
            spdlog::critical("Unable to load secondary font.");
            return false;
        }
    }
    {
        static const ImWchar icons_ranges[] = { ICON_MIN_FAB, ICON_MAX_FAB, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        if (!ImGui::GetIO().Fonts->AddFontFromFileTTF( FONT_ICON_FILE_NAME_FAB, 12, &icons_config, icons_ranges)) {
            spdlog::critical("Unable to load tertiary font.");
            return false;
        }
    }
    auto &style = ImGui::GetStyle();
    style.WindowBorderSize = 1;
    style.FrameBorderSize = 1;
    style.FrameRounding = 3.f;
    style.ChildRounding = 3.f;
    style.ScrollbarRounding = 3.f;
    style.WindowRounding = 3.f;
    return true;
}

int main() {
    SDL_SetMainReady();
    spdlog::set_level(spdlog::level::debug);
    const glm::ivec2 initial_framebuffer_size { 932, 768 };
    DEFER({
        spdlog::debug("Terminating SDL...");
        SDL_Quit();
    });
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    const auto window = SDL_CreateWindow("Visor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, initial_framebuffer_size.x, initial_framebuffer_size.y, SDL_WINDOW_OPENGL);
    if (!window) return 2;
    DEFER({
        spdlog::debug("Destroying main window...");
        SDL_DestroyWindow(window);
    });
    const auto gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) return 3;
    DEFER({
        spdlog::debug("Destroying GL context...");
        SDL_GL_DeleteContext(gl_context);
    });
    if (SDL_GL_MakeCurrent(window, gl_context) != 0) return 4;
    glbinding::initialize(sc_get_proc_address, false);
    spdlog::info("GL: {} ({})", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    const auto imgui_ctx = ImGui::CreateContext();
    if (!imgui_ctx) return 5;
    DEFER({
        spdlog::debug("Destroying ImGui context...");
        ImGui::DestroyContext(imgui_ctx);
    });
    if (!ImGui_ImplSDL2_InitForOpenGL(window, gl_context)) return 6;
    DEFER({
        spdlog::debug("Shutting down ImGui SDL...");
        ImGui_ImplSDL2_Shutdown();
    });
    if (!ImGui_ImplOpenGL3_Init("#version 130")) return 7;
    DEFER({
        spdlog::debug("Shutting down ImGui OpenGL3...");
        ImGui_ImplOpenGL3_Shutdown();
    });
    if (!prepare_styling()) return 8;
    ImDrawCompare drawCmp;
    ImFreetypeEnablement freetype;
    glm::ivec2 recent_framebuffer_size { 0, 0 };
    SDL_JoystickEventState(SDL_ENABLE);
    sc::hidhide::list_devices();
    for (;;) {
        {
            bool should_quit = false;
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    spdlog::critical("Got quit event.");
                    should_quit = true;
                } else if (event.type == SDL_JOYAXISMOTION) {
                    spdlog::info("JOY axis: {}, {}, {}", event.jaxis.which, event.jaxis.axis, static_cast<int>(event.jaxis.value) + 32768);
                } else if (event.type == SDL_JOYHATMOTION) {
                    spdlog::info("JOY hat: {}, {}, {}", event.jhat.which, event.jhat.hat, event.jhat.value);
                } else if (event.type == SDL_JOYBUTTONDOWN) {
                    spdlog::info("JOY button: {}, {}, {}", event.jbutton.which, event.jbutton.button, event.jbutton.state);
                } else if (event.type == SDL_JOYDEVICEADDED) {
                    spdlog::info("JOY added: #{}", event.jdevice.which);
                    const auto joystick = SDL_JoystickOpen(event.jdevice.which);
                    if (joystick == nullptr) continue;
                    const auto serial = SDL_JoystickGetSerial(joystick);
                    const auto guid = SDL_JoystickGetGUID(joystick);
                    const auto device_guid = SDL_JoystickGetGUID(joystick);
                    spdlog::info("Opened joystick #{}: {} -- {} buttons, {} axes // vendor: {}, product: {}, version: {}, instance: {} -- {} // -> {}",
                        event.jdevice.which,
                        SDL_JoystickName(joystick),
                        SDL_JoystickNumButtons(joystick),
                        SDL_JoystickNumAxes(joystick),
                        SDL_JoystickGetVendor(joystick),
                        SDL_JoystickGetProduct(joystick),
                        SDL_JoystickGetProductVersion(joystick),
                        SDL_JoystickInstanceID(joystick),
                        serial ? serial : "????",
                        SDL_JoystickInstanceID(joystick)
                    );
                    for (int i = 0; i < sizeof(guid.data); i++) {
                        fmt::print("{} ", static_cast<int>(guid.data[i]));
                    }
                    fmt::print("\n");
                    for (int i = 0; i < sizeof(device_guid.data); i++) {
                        fmt::print("{} ", static_cast<int>(device_guid.data[i]));
                    }
                    fmt::print("\n");
                } else if (event.type == SDL_JOYDEVICEREMOVED) {
                    spdlog::info("JOY removed: #{}", event.jdevice.which);
                }
            }
            if (should_quit) break;
        }
        const auto hz = [&]() -> std::optional<int> {
            const auto display_i = SDL_GetWindowDisplayIndex(window);
            if (display_i < 0) return 60;
            SDL_DisplayMode mode;
            if (SDL_GetCurrentDisplayMode(display_i, &mode) != 0) return 60;
            return mode.refresh_rate;
        }();
        {
            static auto last_hz = hz;
            if (hz && hz && *hz != *last_hz) spdlog::debug("Hz: {} -> {}", *last_hz, *hz);
            last_hz = hz;
        }
        const auto current_framebuffer_size = [&]() -> glm::ivec2 {
            int dw, dh;
            SDL_GL_GetDrawableSize(window, &dw, &dh);
            return { dw, dh };
        }();
        freetype.preNewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        sc::visor::emit_ui(current_framebuffer_size);
        ImGui::Render();
        const auto draw_data = ImGui::GetDrawData();
        const bool draw_data_changed = !drawCmp.Check(draw_data);
        const bool framebuffer_size_changed = (current_framebuffer_size.x != recent_framebuffer_size.x || current_framebuffer_size.y != recent_framebuffer_size.y);
        const bool needRedraw = framebuffer_size_changed || draw_data_changed;
        if (!needRedraw) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / (hz ? *hz : 60)));
            continue;
        }
        if (framebuffer_size_changed) {
            spdlog::info("Framebuffer: {} -> {}, {} -> {}", recent_framebuffer_size.x, current_framebuffer_size.x, recent_framebuffer_size.y, current_framebuffer_size.y);
            glViewport(0, 0, current_framebuffer_size.x, current_framebuffer_size.y);
            recent_framebuffer_size.x = current_framebuffer_size.x;
            recent_framebuffer_size.y = current_framebuffer_size.y;
        }
        spdlog::info("Rendering.");
        glClearColor(.4, .6, .8, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
        SDL_GL_SwapWindow(window);
    }
    return 0;
}