#include <iostream> // Input/output stream
#include <sstream> // String stream
#include <optional> // Optional values
#include <string> // Strings
#include <functional> // Function objects

#include <tl/expected.hpp> // Expected type for error handling

#include <glm/glm.hpp> // OpenGL Mathematics library

#include <spdlog/spdlog.h> // Logging library
#include <spdlog/sinks/stdout_color_sinks.h> // Console sink for spdlog

#include "../defer.hpp" // Custom defer implementation for resource cleanup

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used Windows headers

#include <windows.h> // Windows API

#undef min
#undef max

#include <imgui.h> // ImGui library

#ifdef SC_FEATURE_ENHANCED_FONTS
    #include "../imgui/imgui_freetype.h" // ImGui FreeType integration for enhanced fonts
#endif

#ifdef SC_FEATURE_SYSTEM_TRAY
    #include "../systray/systray.h" // System tray integration
#endif

#include "../imgui/imgui_impl_opengl3.h" // ImGui OpenGL backend
#include "../imgui/imgui_impl_sdl.h" // ImGui SDL backend
#include "../imgui/imgui_utils.hpp" // ImGui utility functions

#include "../font/imgui.h" // Custom ImGui font rendering

#include "gl-proc-address.h" // OpenGL procedure address retrieval
#include "gl-dbg-msg-cb.h" // OpenGL debug message callback

#include <glbinding/gl33core/gl.h> // OpenGL binding library

using namespace gl;

#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h> // Simple DirectMedia Layer library

static std::optional<std::string> _sc_bootstrap(std::function<std::optional<std::string>(SDL_Window *, ImGuiContext *)> success_cb) {
    SDL_SetMainReady(); // Initialize SDL main subsystems
    const glm::ivec2 initial_framebuffer_size { 932, 768 }; // Initial size of the framebuffer
    DEFER({
        spdlog::debug("Terminating SDL subsystems...");
        SDL_QuitSubSystem(SDL_INIT_EVERYTHING); // Quit all SDL subsystems
        spdlog::debug("Terminating SDL...");
        SDL_Quit(); // Quit SDL
    });
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1"); // Allow background joystick events
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return "Failed to initialize SDL."; // Initialize SDL video subsystem
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3); // Set the major version of the OpenGL context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3); // Set the minor version of the OpenGL context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); // Set the OpenGL context profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG); // Enable OpenGL debug context
    const auto sdl_window = SDL_CreateWindow(SC_APP_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, initial_framebuffer_size.x, initial_framebuffer_size.y, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); // Create SDL window
    if (!sdl_window) return "Failed to create window."; // Window creation failed
    DEFER({
        spdlog::debug("Destroying main window...");
        SDL_DestroyWindow(sdl_window); // Destroy SDL window
    });
    const auto gl_context = SDL_GL_CreateContext(sdl_window); // Create OpenGL context
    if (!gl_context) return "Failed to create OpenGL context."; // OpenGL context creation failed
    DEFER({
        spdlog::debug("Destroying GL context...");
        SDL_GL_DeleteContext(gl_context); // Delete OpenGL context
    });
    if (SDL_GL_MakeCurrent(sdl_window, gl_context) != 0) return "Failed to activate OpenGL context"; // Activate OpenGL context
    glbinding::initialize(sc::boot::gl::get_proc_address, false); // Initialize glbinding library for OpenGL bindings
    glDebugMessageCallback(sc::boot::gl::debug_message_callback, nullptr); // Set the debug message callback for OpenGL
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // Enable synchronous debug output
    spdlog::debug("GL: {} ({})", glGetString(GL_VERSION), glGetString(GL_RENDERER)); // Print OpenGL version and renderer information
    const auto imgui_ctx = ImGui::CreateContext(); // Create ImGui context
    if (!imgui_ctx) return "Failed to create ImGui context."; // ImGui context creation failed
    DEFER({
        spdlog::debug("Destroying ImGui context...");
        ImGui::DestroyContext(imgui_ctx); // Destroy ImGui context
    });
    if (!ImGui_ImplSDL2_InitForOpenGL(sdl_window, gl_context)) return "Failed to prepare ImGui SDL implementation."; // Initialize ImGui for SDL and OpenGL
    DEFER({
        spdlog::debug("Shutting down ImGui SDL...");
        ImGui_ImplSDL2_Shutdown(); // Shutdown ImGui SDL implementation
    });
    if (!ImGui_ImplOpenGL3_Init("#version 130")) return "Failed to prepare ImGui OpenGL implementation."; // Initialize ImGui for OpenGL
    DEFER({
        spdlog::debug("Shutting down ImGui OpenGL3...");
        ImGui_ImplOpenGL3_Shutdown(); // Shutdown ImGui OpenGL implementation
    });
    #ifdef SC_FEATURE_ENHANCED_FONTS
        if (!sc::font::imgui::load(16)) return "Failed to load fonts."; // Load enhanced fonts with ImGui
    #endif
    spdlog::info("Bootstrapping completed."); // Bootstrap process completed
    if (const auto cb_err = success_cb(sdl_window, imgui_ctx /* , vigem_client, vigem_pad */); cb_err.has_value()) {
        spdlog::warn("Bootstrap success routine returned an error: {}", *cb_err);
        return cb_err; // Return error from success callback
    }
    return std::nullopt; // Bootstrap success
}

namespace sc::boot {

    static std::optional<std::string> on_startup();
    static tl::expected<bool, std::string> on_system_event(const SDL_Event &event);
    static tl::expected<bool, std::string> on_fixed_update();
    static tl::expected<bool, std::string> on_update(const glm::ivec2 &framebuffer_size);
    static void on_shutdown();
}

static tl::expected<bool, std::string> _sc_sdl_process_events(SDL_Window *sdl_window) {
    bool should_quit = false; // Flag to indicate if the application should quit
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event); // Process ImGui events
        if (const auto res = sc::boot::on_system_event(event); res.has_value()) {
            if (!*res) should_quit = true; // Check if the system event handler indicates quitting
        } else return tl::make_unexpected(res.error()); // Return unexpected result for system event handling
    }
    if (should_quit) {
        SDL_HideWindow(sdl_window); // Hide the SDL window
        return false; // Quit the application
    }
    return true; // Continue processing events
}

static std::optional<std::string> _sc_run(SDL_Window *sdl_window, ImGuiContext *imgui_ctx) {
    DEFER(sc::boot::on_shutdown()); // Defer shutdown function to ensure cleanup
    if (const auto res = sc::boot::on_startup(); res.has_value()) return *res; // Call startup function and check for errors
    #ifdef SC_FEATURE_SYSTEM_TRAY
        sc::systray::enable([sdl_window]() {
            SDL_ShowWindow(sdl_window); // Show the SDL window when system tray icon is clicked
            SDL_RestoreWindow(sdl_window); // Restore the SDL window if minimized
        });
        DEFER(sc::systray::disable()); // Defer system tray disable function to ensure cleanup
    #endif
    #ifdef SC_FEATURE_MINIMAL_REDRAW
        ImDrawCompare im_draw_cache; // Cache for minimizing redraws
    #endif
    #ifdef SC_FEATURE_ENHANCED_FONTS
        ImFreetypeEnablement freetype; // Enablement for enhanced fonts with FreeType
    #endif
    glm::ivec2 recent_framebuffer_size { 0, 0 }; // Size of the recent framebuffer
    SDL_JoystickEventState(SDL_ENABLE); // Enable joystick events
    for (;;) {
        if (const auto res = _sc_sdl_process_events(sdl_window); res.has_value()) {
            if (!*res) {
                spdlog::warn("Quit signalled."); // Quit the application
                break;
            }
        } else return res.error(); // Return error from SDL event processing
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
        if (const auto res = sc::boot::on_update(current_framebuffer_size); res.has_value()) {
            if (!*res) {
                spdlog::warn("Application wants to exit."); // Application wants to exit
                break;
            }
        } else return res.error(); // Return error from application update
        ImGui::Render();
        const auto draw_data = ImGui::GetDrawData();
        const bool framebuffer_size_changed = (current_framebuffer_size.x != recent_framebuffer_size.x || current_framebuffer_size.y != recent_framebuffer_size.y);
        #ifdef SC_FEATURE_MINIMAL_REDRAW
            const bool draw_data_changed = !im_draw_cache.Check(draw_data);
            const bool need_redraw = framebuffer_size_changed || draw_data_changed;
            if (!need_redraw) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 / (hz ? *hz : 60))); // Sleep to maintain target frame rate
                continue;
            }
        #endif
        if (framebuffer_size_changed) {
            spdlog::debug("Framebuffer resized: {} -> {}, {} -> {}", recent_framebuffer_size.x, current_framebuffer_size.x, recent_framebuffer_size.y, current_framebuffer_size.y);
            glViewport(0, 0, current_framebuffer_size.x, current_framebuffer_size.y); // Set the OpenGL viewport
            recent_framebuffer_size.x = current_framebuffer_size.x;
            recent_framebuffer_size.y = current_framebuffer_size.y;
        }
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        if (static bool shown_window = false; !shown_window) {
            spdlog::debug("First render complete. Making window visible.");
            SDL_ShowWindow(sdl_window); // Show the SDL window after the first render
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

#include "../sentry/sentry.h" // Sentry integration for error reporting

#include <pystring.h> // Python-like string manipulation library

int main() {
    #ifdef SC_FEATURE_SENTRY
        #ifdef NDEBUG
            sc::sentry::initialize(SC_SENTRY_DSN, fmt::format("{}@{}", pystring::lower(SC_APP_NAME), SC_APP_VER).data()); // Initialize Sentry for error reporting
            DEFER(sc::sentry::shutdown()); // Defer Sentry shutdown function to ensure cleanup
        #endif
    #endif
    spdlog::set_default_logger(spdlog::stdout_color_mt(pystring::lower(SC_APP_NAME))); // Set the default logger for spdlog
    spdlog::set_level(spdlog::level::debug); // Set the log level to debug
    #ifndef NDEBUG
        spdlog::critical("This application has been built in DEBUG mode!"); // Log a critical message for debug builds
    #endif
    spdlog::info("Program version: {}", SC_APP_VER); // Log the program version
    if (const auto err = _sc_bootstrap(_sc_run); err.has_value()) {
        spdlog::error("An error has occurred: {}", *err); // Log the error message
        std::stringstream ss;
        ss << "This program experienced an error and was unable to continue running:";
        ss << std::endl << std::endl;
        ss << err.value().data();
        #ifndef SC_FEATURE_NO_ERROR_MESSAGE
            MessageBoxA(NULL, ss.str().data(), fmt::format("{} Error", SC_APP_NAME).data(), MB_OK | MB_ICONERROR); // Show an error message box with the error message
        #endif
        return 1; // Return an error code
    }
    return 0; // Return success code
}
