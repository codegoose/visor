#include <iostream>
#include <sstream>
#include <optional>
#include <string>
#include <functional>

#include <tl/expected.hpp>

#include <glm/glm.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "../defer.hpp"

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#undef min
#undef max

#include <imgui.h>

#ifdef SC_FEATURE_ENHANCED_FONTS

    #include "../imgui/imgui_freetype.h"

#endif

#ifdef SC_FEATURE_SYSTEM_TRAY

    #include "../systray/systray.h"

#endif

#include "../imgui/imgui_impl_opengl3.h"
#include "../imgui/imgui_impl_sdl.h"
#include "../imgui/imgui_utils.hpp"

#include "../font/imgui.h"

static std::optional<std::string> imgui_prepare_styling() {
    #ifdef SC_FEATURE_ENHANCED_FONTS
        if (!sc::font::imgui::load(16)) return "Failed to load fonts.";
    #endif
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

#include "gl-proc-address.h"

#include <glbinding/gl33core/gl.h>

using namespace gl;

#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>

static std::optional<std::string> bootstrap(std::function<std::optional<std::string>(SDL_Window *, ImGuiContext *)> success_cb) {
        SDL_SetMainReady();
        const glm::ivec2 initial_framebuffer_size { 932, 768 };
        DEFER({
            spdlog::debug("Terminating SDL subsystems...");
            SDL_QuitSubSystem(SDL_INIT_EVERYTHING);
            spdlog::debug("Terminating SDL...");
            SDL_Quit();
        });
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) return "Failed to initialize SDL.";
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        const auto sdl_window = SDL_CreateWindow(SC_APP_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, initial_framebuffer_size.x, initial_framebuffer_size.y, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        if (!sdl_window) return "Failed to create window.";
        DEFER({
            spdlog::debug("Destroying main window...");
            SDL_DestroyWindow(sdl_window);
        });
    const auto gl_context = SDL_GL_CreateContext(sdl_window);
    if (!gl_context) return "Failed to create OpenGL context.";
    DEFER({
        spdlog::debug("Destroying GL context...");
        SDL_GL_DeleteContext(gl_context);
    });
    if (SDL_GL_MakeCurrent(sdl_window, gl_context) != 0) return "Failed to activate OpenGL context";
    glbinding::initialize(sc::boot::gl::get_proc_address, false);
    spdlog::debug("GL: {} ({})", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    const auto imgui_ctx = ImGui::CreateContext();
    if (!imgui_ctx) return "Failed to create ImGui context.";
    DEFER({
        spdlog::debug("Destroying ImGui context...");
        ImGui::DestroyContext(imgui_ctx);
    });
    if (!ImGui_ImplSDL2_InitForOpenGL(sdl_window, gl_context)) return "Failed to prepare ImGui SDL implementation.";
    DEFER({
        spdlog::debug("Shutting down ImGui SDL...");
        ImGui_ImplSDL2_Shutdown();
    });
    if (!ImGui_ImplOpenGL3_Init("#version 130")) return "Failed to prepare ImGui OpenGL implementation.";
    DEFER({
        spdlog::debug("Shutting down ImGui OpenGL3...");
        ImGui_ImplOpenGL3_Shutdown();
    });
    if (const auto styling_err = imgui_prepare_styling(); styling_err.has_value()) return styling_err;
    spdlog::info("Bootstrapping completed.");
    if (const auto cb_err = success_cb(sdl_window, imgui_ctx /* , vigem_client, vigem_pad */); cb_err.has_value()) {
        spdlog::warn("Bootstrap success routine returned an error: {}", *cb_err);
        return cb_err;
    }
    return std::nullopt;
}

static std::optional<std::string> on_startup();
static tl::expected<bool, std::string> on_system_event(const SDL_Event &event);
static tl::expected<bool, std::string> on_fixed_update();
static tl::expected<bool, std::string> on_update(const glm::ivec2 &framebuffer_size);
static void on_shutdown();

static tl::expected<bool, std::string> sdl_process_events(SDL_Window *sdl_window) {
    bool should_quit = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (const auto res = on_system_event(event); res.has_value()) {
            if (!*res) should_quit = true;
        } else return tl::make_unexpected(res.error());
    }
    if (should_quit) {
        SDL_HideWindow(sdl_window);
        return false;
    }
    return true;
}

static std::optional<std::string> run(SDL_Window *sdl_window, ImGuiContext *imgui_ctx) {
    DEFER(on_shutdown());
    if (const auto res = on_startup(); res.has_value()) return *res;
    #ifdef SC_FEATURE_SYSTEM_TRAY
        sc::systray::enable([sdl_window]() {
            SDL_ShowWindow(sdl_window);
            SDL_RestoreWindow(sdl_window);
        });
        DEFER(sc::systray::disable());
    #endif
    #ifdef SC_FEATURE_MINIMAL_REDRAW
        ImDrawCompare im_draw_cache;
    #endif
    #ifdef SC_FEATURE_ENHANCED_FONTS
        ImFreetypeEnablement freetype;
    #endif
    glm::ivec2 recent_framebuffer_size { 0, 0 };
    SDL_JoystickEventState(SDL_ENABLE);
    for (;;) {
        if (const auto res = sdl_process_events(sdl_window); res.has_value()) {
            if (!*res) {
                spdlog::warn("Quit signalled.");
                break;
            }
        } else return res.error();
        const auto hz = [&]() -> std::optional<int> {
            const auto display_i = SDL_GetWindowDisplayIndex(sdl_window);
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
            SDL_GL_GetDrawableSize(sdl_window, &dw, &dh);
            return { dw, dh };
        }();
        #ifdef SC_FEATURE_ENHANCED_FONTS
            freetype.PreNewFrame();
        #endif
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        on_update(current_framebuffer_size);
        ImGui::Render();
        const auto draw_data = ImGui::GetDrawData();
        const bool framebuffer_size_changed = (current_framebuffer_size.x != recent_framebuffer_size.x || current_framebuffer_size.y != recent_framebuffer_size.y);
        #ifdef SC_FEATURE_MINIMAL_REDRAW
            const bool draw_data_changed = !im_draw_cache.Check(draw_data);
            const bool need_redraw = framebuffer_size_changed || draw_data_changed;
            if (!need_redraw) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 / (hz ? *hz : 60)));
                continue;
            }
        #endif
        if (framebuffer_size_changed) {
            spdlog::debug("Framebuffer resized: {} -> {}, {} -> {}", recent_framebuffer_size.x, current_framebuffer_size.x, recent_framebuffer_size.y, current_framebuffer_size.y);
            glViewport(0, 0, current_framebuffer_size.x, current_framebuffer_size.y);
            recent_framebuffer_size.x = current_framebuffer_size.x;
            recent_framebuffer_size.y = current_framebuffer_size.y;
        }
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        if (static bool shown_window = false; !shown_window) {
            spdlog::debug("First render complete. Making window visible.");
            SDL_ShowWindow(sdl_window);
            shown_window = true;
        }
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
        SDL_GL_SwapWindow(sdl_window);
        static uint64_t num_frame_i = 0;
        spdlog::debug("Rendered frame #{}.", num_frame_i);
        num_frame_i++;
    }
    return std::nullopt;
}

#include "../sentry/sentry.h"

#include <pystring.h>

int main() {
    #ifdef SC_FEATURE_SENTRY
        #ifdef NDEBUG
            sc::sentry::initialize(SC_SENTRY_DSN, fmt::format("{}@{}", pystring::lower(SC_APP_NAME), SC_APP_VER).data());
            DEFER(sc::sentry::shutdown());
        #endif
    #endif
    spdlog::set_default_logger(spdlog::stdout_color_mt(pystring::lower(SC_APP_NAME)));
    spdlog::set_level(spdlog::level::debug);
    #ifndef NDEBUG
        spdlog::critical("This application has been built in DEBUG mode!");
    #endif
    spdlog::info("Program version: {}", SC_APP_VER);
    if (const auto err = bootstrap(run); err.has_value()) {
        spdlog::error("An error has occurred: {}", *err);
        std::stringstream ss;
        ss << "This program experienced an error and was unable to continue running:";
        ss << std::endl << std::endl;
        ss << err.value().data();
        #ifndef SC_FEATURE_NO_ERROR_MESSAGE
            MessageBoxA(NULL, ss.str().data(), fmt::format("{} Error", SC_APP_NAME).data(), MB_OK | MB_ICONERROR);
        #endif
        return 1;
    }
    return 0;
}