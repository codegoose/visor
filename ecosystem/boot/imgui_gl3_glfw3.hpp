#pragma message(SC_APP_NAME)
#pragma message(SC_APP_VER)
#pragma message("[EON] Using GLFW/OpenGL33/ImGui.")

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
#include "../imgui/imgui_impl_glfw.h"
#include "../imgui/imgui_utils.hpp"

#include "../font/imgui.h"

#include "gl-proc-address.h"
#include "gl-dbg-msg-cb.h"

#include <glbinding/gl33core/gl.h>

using namespace gl;

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>

#ifdef SC_FEATURE_TRANSPARENT_WINDOW

    #include <GLFW/glfw3native.h>

#endif

static glm::ivec2 _sc_current_framebuffer_size = { 0, 0 };

static void _sc_glfw_window_resize_cb(GLFWwindow *window, int w, int h);

static std::optional<std::string> _sc_bootstrap(std::function<std::optional<std::string>(GLFWwindow *, ImGuiContext *)> success_cb) {
    #if defined(SC_VIEW_INIT_W) && defined(SC_VIEW_INIT_H)
        const glm::ivec2 initial_framebuffer_size { SC_VIEW_INIT_W, SC_VIEW_INIT_H };
        #pragma message("[EON] Using custom initial view dimensions.")
    #else
        const glm::ivec2 initial_framebuffer_size { 640, 480 };
    #endif
    DEFER({
        spdlog::debug("Terminating GLFW...");
        glfwTerminate();
    });
    if (glfwInit() == GLFW_FALSE) return "Failed to initialize GLFW.";
    #ifdef SC_FEATURE_TRANSPARENT_WINDOW
        #pragma message("[EON] Using transparent framebuffer.")
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    #endif
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    const auto glfw_window = glfwCreateWindow(initial_framebuffer_size.x, initial_framebuffer_size.y, SC_APP_NAME, nullptr, nullptr);
    if (!glfw_window) return "Failed to create window.";
    DEFER({
        spdlog::debug("Destroying main window...");
        glfwDestroyWindow(glfw_window);
    });
    #if defined(SC_VIEW_MIN_W) && defined(SC_VIEW_MIN_H)
    glfwSetWindowSizeLimits(glfw_window, SC_VIEW_MIN_W, SC_VIEW_MIN_H, GLFW_DONT_CARE, GLFW_DONT_CARE);
    #endif
    #ifdef SC_FEATURE_RENDER_ON_RESIZE
        #pragma message("[EON] Using rendering within window size callback.")
        glfwSetFramebufferSizeCallback(glfw_window, _sc_glfw_window_resize_cb);
    #endif
    glfwMakeContextCurrent(glfw_window);
    glbinding::initialize(sc::boot::gl::get_proc_address, false);
    glDebugMessageCallback(sc::boot::gl::debug_message_callback, nullptr);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    spdlog::debug("GL: {} ({})", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    const auto imgui_ctx = ImGui::CreateContext();
    if (!imgui_ctx) return "Failed to create ImGui context.";
    DEFER({
        spdlog::debug("Destroying ImGui context...");
        ImGui::DestroyContext(imgui_ctx);
    });
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (!ImGui_ImplGlfw_InitForOpenGL(glfw_window, true)) return "Failed to prepare ImGui GLFW implementation.";
    DEFER({
        spdlog::debug("Shutting down ImGui GLFW...");
        ImGui_ImplGlfw_Shutdown();
    });
    if (!ImGui_ImplOpenGL3_Init("#version 130")) return "Failed to prepare ImGui OpenGL implementation.";
    DEFER({
        spdlog::debug("Shutting down ImGui OpenGL3...");
        ImGui_ImplOpenGL3_Shutdown();
    });
    #ifdef SC_FEATURE_ENHANCED_FONTS
        #pragma message("[EON] Using Freetype to enhance fonts.")
        if (!sc::font::imgui::load(16)) return "Failed to load fonts.";
    #endif
    spdlog::info("Bootstrapping completed.");
    if (const auto cb_err = success_cb(glfw_window, imgui_ctx /* , vigem_client, vigem_pad */); cb_err.has_value()) {
        spdlog::warn("Bootstrap success routine returned an error: {}", *cb_err);
        return cb_err;
    }
    return std::nullopt;
}

namespace sc::boot {

    static std::optional<std::string> on_startup();
    static tl::expected<bool, std::string> on_fixed_update();
    static tl::expected<bool, std::string> on_update(const glm::ivec2 &framebuffer_size);
    static void on_shutdown();
}

static tl::expected<bool, std::string> _sc_glfw_process_events(GLFWwindow *glfw_window) {
    glfwPollEvents();
    if (glfwWindowShouldClose(glfw_window)) {
        glfwHideWindow(glfw_window);
        return false;
    }
    return true;
}

static void _sc_glfw_render() {
    const auto glfw_window = glfwGetCurrentContext();
    const auto draw_data = ImGui::GetDrawData();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (static bool shown_window = false; !shown_window) {
        spdlog::debug("First render complete. Making window visible.");
        glfwShowWindow(glfw_window);
        shown_window = true;
    }
    ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    glfwSwapBuffers(glfw_window);
}

static tl::expected<bool, std::string> _sc_imgui_render() {
    #ifdef SC_FEATURE_ENHANCED_FONTS
        static ImFreetypeEnablement freetype;
        freetype.PreNewFrame();
    #endif
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (const auto res = sc::boot::on_update(_sc_current_framebuffer_size); res.has_value()) {
        if (!*res) {
            spdlog::warn("Application wants to exit.");
            glfwHideWindow(glfwGetCurrentContext());
            return false;
        }
    } else return tl::make_unexpected(res.error());
    ImGui::Render();
    return true;
}

static void _sc_glfw_window_resize_cb(GLFWwindow *window, int w, int h) {
    _sc_current_framebuffer_size = { w, h };
    glViewport(0, 0, w, h);
    _sc_imgui_render();
    _sc_glfw_render();
}

static std::optional<std::string> _sc_run(GLFWwindow *glfw_window, ImGuiContext *imgui_ctx) {
    DEFER(sc::boot::on_shutdown());
    if (const auto res = sc::boot::on_startup(); res.has_value()) return *res;
    #ifdef SC_FEATURE_SYSTEM_TRAY
        #pragma message("[EON] Using system tray.")
        sc::systray::enable([glfw_window]() {
            glfwShowWindow(glfw_window);
            glfwRestoreWindow(glfw_window);
        });
        DEFER(sc::systray::disable());
    #endif
    #ifdef SC_FEATURE_MINIMAL_REDRAW
        #pragma message("[EON] Using redraw minimization.")
        ImDrawCompare im_draw_cache;
    #endif
    glm::ivec2 recent_framebuffer_size { 0, 0 };
    for (;;) {
        if (const auto res = _sc_glfw_process_events(glfw_window); res.has_value()) {
            if (!*res) {
                spdlog::warn("Quit signalled.");
                break;
            }
        } else return res.error();
        const auto hz = [&]() -> std::optional<int> {
            int num_monitors;
            const auto monitors = glfwGetMonitors(&num_monitors);
            if (num_monitors > 0) {
                int wx, wy;
                glfwGetWindowPos(glfw_window, &wx, &wy);
                int ww, wh;
                glfwGetWindowSize(glfw_window, &ww, &wh);
                if (ww > 0 && wh > 0) {
                    for (int i = 0; i < num_monitors; i++) {
                        int dx, dy;
                        glfwGetMonitorPos(monitors[i], &dx, &dy);
                        const auto video_mode = glfwGetVideoMode(monitors[i]);
                        if (!video_mode) continue;
                        const int wcx = wx + (ww / 2);
                        const int wcy = wy + (wh / 2);
                        if (wcx >= dx && wcx < dx + video_mode->width) {
                            if (wcy >= dy && wcy < dy + video_mode->height) {
                                return video_mode->refreshRate;
                            }
                        }
                    }
                }
            }
            return 60;
        }();
        {
            static auto last_hz = hz;
            if (hz && hz && *hz != *last_hz) spdlog::debug("Changing refresh rate to {}/second.", *hz);
            last_hz = hz;
        }
        {
            static auto last_info = std::chrono::system_clock::now();
            const auto now = std::chrono::system_clock::now();
            if (hz && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_info).count() > 3000) {
                spdlog::debug("Refresh rate is {}/second.", *hz);
                last_info = now;
            }            
        }
        _sc_current_framebuffer_size = [&]() -> glm::ivec2 {
            int dw, dh;
            glfwGetFramebufferSize(glfw_window, &dw, &dh);
            return { dw, dh };
        }();
        if (const auto res = _sc_imgui_render(); res.has_value()) {
            if (!*res) break;
        }
        else return res.error();
        const auto draw_data = ImGui::GetDrawData();
        const bool framebuffer_size_changed = (_sc_current_framebuffer_size.x != recent_framebuffer_size.x || _sc_current_framebuffer_size.y != recent_framebuffer_size.y);
        #ifdef SC_FEATURE_MINIMAL_REDRAW
            const bool draw_data_changed = !im_draw_cache.Check(draw_data);
            const bool need_redraw = framebuffer_size_changed || draw_data_changed;
            if (!need_redraw) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 / (hz ? *hz : 60)));
                continue;
            }
        #endif
        if (framebuffer_size_changed) {
            spdlog::debug("Framebuffer resized: {} -> {}, {} -> {}", recent_framebuffer_size.x, _sc_current_framebuffer_size.x, recent_framebuffer_size.y, _sc_current_framebuffer_size.y);
            glViewport(0, 0, _sc_current_framebuffer_size.x, _sc_current_framebuffer_size.y);
            recent_framebuffer_size.x = _sc_current_framebuffer_size.x;
            recent_framebuffer_size.y = _sc_current_framebuffer_size.y;
        }
        _sc_glfw_render();
        static uint64_t num_frame_i = 0;
        num_frame_i++;
    }
    return std::nullopt;
}

#include "../sentry/sentry.h"

#include <pystring.h>

static int _sc_entry_point() {
    #ifdef SC_FEATURE_SENTRY
        #ifdef NDEBUG
            #pragma message("[EON] Using Sentry.")
            sc::sentry::initialize(SC_SENTRY_DSN, fmt::format("{}@{}", pystring::lower(SC_APP_NAME), SC_APP_VER).data());
            DEFER(sc::sentry::shutdown());
        #else
        #pragma message("[EON] Sentry is disabled due for debug build.")
        #endif
    #endif
    spdlog::set_default_logger(spdlog::stdout_color_mt(pystring::lower(SC_APP_NAME)));
    spdlog::set_level(spdlog::level::debug);
    #ifndef NDEBUG
        spdlog::critical("This application has been built in DEBUG mode!");
    #endif
    spdlog::info("Program version: {}", SC_APP_VER);
    if (const auto err = _sc_bootstrap(_sc_run); err.has_value()) {
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

#ifdef NDEBUG
#pragma message("Using WinMain.")
int WinMain(HINSTANCE _instance, HINSTANCE _prev_instance, PSTR _command_line, int _command_show) {
#else
int main() {
#endif
    return _sc_entry_point();
}