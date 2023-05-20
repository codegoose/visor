#pragma message(SC_APP_NAME) // Display a message during compilation with the value of SC_APP_NAME
#pragma message(SC_APP_VER) // Display a message during compilation with the value of SC_APP_VER
#pragma message("[EON] Using GLFW/OpenGL33/ImGui.") // Display a message during compilation indicating the libraries being used

#include <iostream>
#include <sstream>
#include <optional>
#include <string>
#include <functional>

#include <tl/expected.hpp>

#include <argparse/argparse.hpp> // Include the library for command-line argument parsing

static argparse::ArgumentParser program(VER_APP_NAME, VER_APP_VER); // Create an ArgumentParser object named "program" with the application name and version

#include <glm/glm.hpp> // Include the library for mathematical operations using vectors and matrices

#include <spdlog/spdlog.h> // Include the library for logging
#include <spdlog/sinks/stdout_color_sinks.h> // Include the library for logging to the console with colors

#include "../defer.hpp" // Include a custom header for deferred execution of code

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used Windows headers

#include <windows.h> // Include the Windows API header

#undef min // Undefine the 'min' macro to avoid conflicts
#undef max // Undefine the 'max' macro to avoid conflicts

#include <imgui.h> // Include the ImGui library

#ifdef SC_FEATURE_ENHANCED_FONTS // Check if SC_FEATURE_ENHANCED_FONTS is defined

    #include "../imgui/imgui_freetype.h" // Include the library for enhanced fonts using Freetype

#endif

#ifndef SC_FONT_SIZE // Check if SC_FONT_SIZE is not defined
#define SC_FONT_SIZE 16 // Define the default font size as 16
#endif

#ifdef SC_FEATURE_SYSTEM_TRAY // Check if SC_FEATURE_SYSTEM_TRAY is defined

    #include "../systray/systray.h" // Include the library for system tray functionality

#endif

#include "../imgui/imgui_impl_opengl3.h" // Include the ImGui library for OpenGL 3 implementation
#include "../imgui/imgui_impl_glfw.h" // Include the ImGui library for GLFW implementation
#include "../imgui/imgui_utils.hpp" // Include the utility functions for ImGui

#include "../font/imgui.h" // Include the custom ImGui font header

#include "gl-proc-address.h" // Include the header for getting OpenGL function addresses
#include "gl-dbg-msg-cb.h" // Include the header for OpenGL debug message callback

#include <glbinding/gl33core/gl.h> // Include the glbinding library for OpenGL 3.3 core profile

using namespace gl; // Use the gl namespace for convenience

#define GLFW_INCLUDE_NONE // Exclude GLFW headers

#include <GLFW/glfw3.h> // Include the GLFW library for window and input handling

static glm::ivec2 _sc_current_framebuffer_size = { 0, 0 }; // Initialize the current framebuffer size as (0, 0)
static bool _sc_force_redraw = false; // Initialize the force redraw flag as false

static void _sc_glfw_window_resize_cb(GLFWwindow *window, int w, int h); // Declare the callback function for GLFW window resize event

static std::optional<std::string> _sc_bootstrap(std::function<std::optional<std::string>(GLFWwindow *, ImGuiContext *)> success_cb) {
    // Bootstrap function for initializing the application
    // success_cb is a callback function to be executed if the bootstrap is successful
    // It returns an optional string indicating an error if the bootstrap fails

    #if defined(SC_VIEW_INIT_W) && defined(SC_VIEW_INIT_H)
        const glm::ivec2 initial_framebuffer_size { SC_VIEW_INIT_W, SC_VIEW_INIT_H };
        #pragma message("[EON] Using custom initial view dimensions.") // Display a message during compilation indicating the use of custom initial view dimensions
    #else
        const glm::ivec2 initial_framebuffer_size { 640, 480 }; // Default initial view dimensions
    #endif

    DEFER({
        spdlog::debug("Terminating GLFW..."); // Log a debug message
        glfwTerminate(); // Terminate GLFW
    });

    if (glfwInit() == GLFW_FALSE) return "Failed to initialize GLFW."; // Initialize GLFW, return an error message if it fails

    #ifdef SC_FEATURE_TRANSPARENT_WINDOW
        #pragma message("[EON] Using transparent framebuffer.") // Display a message during compilation indicating the use of transparent framebuffer
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE); // Set GLFW window hint for transparent framebuffer
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // Set GLFW window hint to not be decorated
    #endif

    #ifdef SC_FEATURE_FLOATING_WINDOW
        #pragma message("[EON] Using floating window.") // Display a message during compilation indicating the use of floating window
        glfwWindowHint(GLFW_FLOATING, GLFW_TRUE); // Set GLFW window hint for floating window
    #endif

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Set GLFW window hint to not be initially visible
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Set GLFW window hint for OpenGL context major version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // Set GLFW window hint for OpenGL context minor version
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Set GLFW window hint for OpenGL core profile
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE); // Set GLFW window hint to enable OpenGL debug context

    #ifdef NDEBUG
    const auto glfw_window_title = fmt::format("{}", VER_APP_NAME, VER_APP_VER); // Create GLFW window title without development build information
    #else
    const auto glfw_window_title = fmt::format("{} v{} (Development Build)", VER_APP_NAME, VER_APP_VER); // Create GLFW window title with development build information
    #endif

    const auto glfw_window = glfwCreateWindow(initial_framebuffer_size.x, initial_framebuffer_size.y, glfw_window_title.data(), nullptr, nullptr); // Create a GLFW window
    if (!glfw_window) return "Failed to create window."; // Return an error message if window creation fails

    DEFER({
        spdlog::debug("Destroying main window..."); // Log a debug message
        glfwDestroyWindow(glfw_window); // Destroy the GLFW window
    });

    #if defined(SC_VIEW_MIN_W) && defined(SC_VIEW_MIN_H)
    glfwSetWindowSizeLimits(glfw_window, SC_VIEW_MIN_W, SC_VIEW_MIN_H, GLFW_DONT_CARE, GLFW_DONT_CARE); // Set the minimum window size limits
    #endif

    #ifdef SC_FEATURE_RENDER_ON_RESIZE
        #pragma message("[EON] Using rendering within window size callback.") // Display a message during compilation indicating the use of rendering within window size callback
        glfwSetFramebufferSizeCallback(glfw_window, _sc_glfw_window_resize_cb); // Set the GLFW framebuffer size callback function
    #endif

    glfwMakeContextCurrent(glfw_window); // Make the GLFW window's context current
    glbinding::initialize(sc::boot::gl::get_proc_address, false); // Initialize the glbinding library for OpenGL function binding
    glDebugMessageCallback(sc::boot::gl::debug_message_callback, nullptr); // Set the OpenGL debug message callback function
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // Enable synchronous OpenGL debug output
    spdlog::debug("GL: {} ({})", glGetString(GL_VERSION), glGetString(GL_RENDERER)); // Log the OpenGL version and renderer information

    const auto imgui_ctx = ImGui::CreateContext(); // Create an ImGui context
    if (!imgui_ctx) return "Failed to create ImGui context."; // Return an error message if ImGui context creation fails

    DEFER({
        spdlog::debug("Destroying ImGui context..."); // Log a debug message
        ImGui::DestroyContext(imgui_ctx); // Destroy the ImGui context
    });

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard navigation in ImGui

    if (!ImGui_ImplGlfw_InitForOpenGL(glfw_window, true)) return "Failed to prepare ImGui GLFW implementation."; // Initialize the ImGui GLFW implementation
    DEFER({
        spdlog::debug("Shutting down ImGui GLFW..."); // Log a debug message
        ImGui_ImplGlfw_Shutdown(); // Shutdown the ImGui GLFW implementation
    });

    if (!ImGui_ImplOpenGL3_Init("#version 130")) return "Failed to prepare ImGui OpenGL implementation."; // Initialize the ImGui OpenGL implementation
    DEFER({
        spdlog::debug("Shutting down ImGui OpenGL3..."); // Log a debug message
        ImGui_ImplOpenGL3_Shutdown(); // Shutdown the ImGui OpenGL implementation
    });

    #ifdef SC_FEATURE_ENHANCED_FONTS
        #pragma message("[EON] Using Freetype to enhance fonts.") // Display a message during compilation indicating the use of enhanced fonts with Freetype
        if (!sc::font::imgui::load(SC_FONT_SIZE)) return "Failed to load fonts."; // Load enhanced fonts with the specified font size
    #endif

    #ifdef SC_FEATURE_CENTER_WINDOW
        #pragma message("[EON] Centering window.") // Display a message during compilation indicating the centering of the window
        if (const auto monitor = glfwGetPrimaryMonitor(); monitor) {
            int mx, my, mw, mh;
            glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);
            if (mw != 0 && mh != 0) {
                int x, y;
                glfwGetWindowPos(glfw_window, &x, &y);
                int w, h;
                glfwGetWindowSize(glfw_window, &w, &h);
                if (w != 0 && h != 0) {
                    glfwSetWindowPos(glfw_window, mx + (mw / 2) - (w / 2), my + (mh / 2) - (h / 2)); // Center the window on the screen
                }
            }
        }
    #endif

    spdlog::info("Bootstrapping completed."); // Log an info message

    if (const auto cb_err = success_cb(glfw_window, imgui_ctx /* , vigem_client, vigem_pad */); cb_err.has_value()) {
        spdlog::warn("Bootstrap success routine returned an error: {}", *cb_err); // Log a warning message
        return cb_err; // Return the error message
    }

    return std::nullopt; // Return no error
}

namespace sc::boot {

    static std::optional<std::string> on_startup();
    static tl::expected<bool, std::string> on_fixed_update();
    static tl::expected<bool, std::string> on_update(const glm::ivec2 &framebuffer_size, bool *const force_redraw = nullptr);
    static void on_shutdown();
}

static tl::expected<bool, std::string> _sc_glfw_process_events(GLFWwindow *glfw_window) {
    // Process GLFW events and return true to continue the application or false to exit
    // glfw_window is the GLFW window to process events for

    glfwPollEvents(); // Poll for events

    if (glfwWindowShouldClose(glfw_window)) {
        glfwHideWindow(glfw_window); // Hide the window

        #ifdef SC_FEATURE_SYSTEM_TRAY
        glfwSetWindowShouldClose(glfw_window, GLFW_FALSE); // Set the window should not close flag
        #else
        return false; // Return false to exit the application
        #endif
    }

    return true; // Return true to continue the application
}

static void _sc_glfw_render() {
    // Render ImGui and swap the GLFW window's buffers

    const auto glfw_window = glfwGetCurrentContext(); // Get the current GLFW window
    const auto draw_data = ImGui::GetDrawData(); // Get the ImGui draw data

    glClearColor(0, 0, 0, 0); // Clear the color buffer
    glClear(GL_COLOR_BUFFER_BIT); // Clear the color buffer

    #ifdef SC_FEATURE_SYSTEM_TRAY
    if (static bool bg = program.get<bool>("--background"); !bg) {
    #else
    {
    #endif
        if (static bool shown_window = false; !shown_window) {
            spdlog::debug("First render complete. Making window visible."); // Log a debug message
            glfwShowWindow(glfw_window); // Show the window
            shown_window = true;
        }
    }

    ImGui_ImplOpenGL3_RenderDrawData(draw_data); // Render the ImGui draw data using OpenGL
    glfwSwapBuffers(glfw_window); // Swap the window's buffers
}

static tl::expected<bool, std::string> _sc_imgui_render(bool *const force_redraw = nullptr) {
    // Render the ImGui UI and handle update callbacks
    // force_redraw is a pointer to a flag indicating whether a redraw is forced

    #ifdef SC_FEATURE_ENHANCED_FONTS
        static ImFreetypeEnablement freetype;
        freetype.PreNewFrame();
    #endif

    ImGui_ImplOpenGL3_NewFrame(); // Start a new ImGui frame for OpenGL
    ImGui_ImplGlfw_NewFrame(); // Start a new ImGui frame for GLFW
    ImGui::NewFrame(); // Start a new ImGui frame

    if (const auto res = sc::boot::on_update(_sc_current_framebuffer_size, force_redraw); res.has_value()) {
        if (!*res) {
            spdlog::warn("Application wants to exit."); // Log a warning message
            glfwHideWindow(glfwGetCurrentContext()); // Hide the window
            return false; // Return false to exit the application
        }
    } else return tl::make_unexpected(res.error());

    ImGui::Render(); // Render the ImGui frame

    return true; // Return true to continue the application
}

static void _sc_glfw_window_resize_cb(GLFWwindow *window, int w, int h) {
    // Callback function for GLFW window resize event
    // window is the GLFW window
    // w is the new width of the window
    // h is the new height of the window

    _sc_current_framebuffer_size = { w, h }; // Update the current framebuffer size

    glViewport(0, 0, w, h); // Set the OpenGL viewport

    _sc_imgui_render(); // Render the ImGui UI
    _sc_glfw_render(); // Render the GLFW window
}

static std::optional<std::string> _sc_run(GLFWwindow *glfw_window, ImGuiContext *imgui_ctx) {
    // Main function for running the application
    // glfw_window is the GLFW window
    // imgui_ctx is the ImGui context

    DEFER(sc::boot::on_shutdown()); // Register the on_shutdown function for deferred execution

    if (const auto res = sc::boot::on_startup(); res.has_value()) return *res; // Call the on_startup function and return an error message if it fails

    #ifdef SC_FEATURE_SYSTEM_TRAY
        #pragma message("[EON] Using system tray.") // Display a message during compilation indicating the use of system tray
        sc::systray::enable([glfw_window]() {
            glfwShowWindow(glfw_window); // Show the window
            glfwRestoreWindow(glfw_window); // Restore the window from minimized or maximized state
            _sc_force_redraw = true; // Set the force redraw flag
        });
        DEFER(sc::systray::disable()); // Register the systray disable function for deferred execution
    #endif

    #ifdef SC_FEATURE_MINIMAL_REDRAW
        #pragma message("[EON] Using redraw minimization.") // Display a message during compilation indicating the use of redraw minimization
        ImDrawCompare im_draw_cache;
    #endif

    glm::ivec2 recent_framebuffer_size { 0, 0 }; // Initialize the recent framebuffer size as (0, 0)

    for (;;) {
        if (const auto res = _sc_glfw_process_events(glfw_window); res.has_value()) {
            if (!*res) {
                spdlog::warn("Quit signalled."); // Log a warning message
                break; // Break the loop to exit the application
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
                                return video_mode->refreshRate; // Return the monitor's refresh rate
                            }
                        }
                    }
                }
            }
            return 60; // Default refresh rate
        }();

        {
            static auto last_hz = hz;
            if (hz && hz && *hz != *last_hz) spdlog::debug("Changing refresh rate to {}/second.", *hz); // Log a debug message
            last_hz = hz;
        }

        {
            static auto last_info = std::chrono::system_clock::now();
            const auto now = std::chrono::system_clock::now();
            if (hz && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_info).count() > 3000) {
                spdlog::debug("Refresh rate is {}/second.", *hz); // Log a debug message
                last_info = now;
            }            
        }

        _sc_current_framebuffer_size = [&]() -> glm::ivec2 {
            int dw, dh;
            glfwGetFramebufferSize(glfw_window, &dw, &dh);
            return { dw, dh };
        }(); // Get the current framebuffer size

        #ifdef SC_FEATURE_MINIMAL_REDRAW
        bool force_redraw = false;
        if (const auto res = _sc_imgui_render(&force_redraw); res.has_value()) {
        #else
        if (const auto res = _sc_imgui_render(); res.has_value()) {
        #endif
            if (!*res) break; // Break the loop to exit the application
        }
        else return res.error();

        const auto draw_data = ImGui::GetDrawData(); // Get the ImGui draw data
        const bool framebuffer_size_changed = (_sc_current_framebuffer_size.x != recent_framebuffer_size.x || _sc_current_framebuffer_size.y != recent_framebuffer_size.y); // Check if the framebuffer size has changed

        #ifdef SC_FEATURE_MINIMAL_REDRAW
            const bool draw_data_changed = !im_draw_cache.Check(draw_data);
            const bool need_redraw = force_redraw || framebuffer_size_changed || draw_data_changed || _sc_force_redraw;
            if (!need_redraw) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 / (hz ? *hz : 60))); // Sleep to reduce the frame rate
                continue; // Continue to the next iteration of the loop
            }
            _sc_force_redraw = false;
        #endif

        if (framebuffer_size_changed) {
            spdlog::debug("Framebuffer resized: {} -> {}, {} -> {}", recent_framebuffer_size.x, _sc_current_framebuffer_size.x, recent_framebuffer_size.y, _sc_current_framebuffer_size.y); // Log a debug message
            glViewport(0, 0, _sc_current_framebuffer_size.x, _sc_current_framebuffer_size.y); // Set the OpenGL viewport
            recent_framebuffer_size.x = _sc_current_framebuffer_size.x;
            recent_framebuffer_size.y = _sc_current_framebuffer_size.y;
        }

        if (_sc_current_framebuffer_size.x && _sc_current_framebuffer_size.y) _sc_glfw_render(); // Render the GLFW window if the framebuffer size is valid

        static uint64_t num_frame_i = 0;
        num_frame_i++;
    }

    return std::nullopt; // Return no error
}

#include "../sentry/sentry.h"

#include <pystring.h>

static void _sc_print_publisher_info(int arg_c, char **arg_v) {
    // Print information about the program and command-line arguments
    // arg_c is the number of command-line arguments
    // arg_v is the array of command-line argument strings

    std::cout << std::endl;
    std::cout << "Copyright " << VER_LEGAL_COPYRIGHT << std::endl;
    std::cout << VER_APP_NAME << " (" << VER_APP_VER << ")" << std::endl;
    std::cout << VER_APP_DESCRIPTION << std::endl;

    #ifdef NDEBUG
    std::cout << "This application was compiled in release mode." << std::endl;
    #else
    std::cout << "This application was compiled in debug mode." << std::endl;
    #endif

    std::cout << "There " << (arg_c == 1 ? "was" : "were") << " " << arg_c << " argument" << (arg_c == 1 ? "" : "s") << " passed." << std::endl;
    for (int i = 0; i < arg_c; i++) std::cout << "  [" << i << "] \"" << arg_v[i] << "\"" << std::endl;

    std::cout << std::endl;
}

static int _sc_entry_point(int arg_c, char **arg_v) {
    #ifdef SC_FEATURE_SYSTEM_TRAY
    program.add_argument("--background").help("start in system tray, not visible").default_value(false).implicit_value(true);
    #endif

    try {
        program.parse_args(arg_c, arg_v);
    } catch (const std::runtime_error &err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return -1; // Return -1 to indicate an error
    }

    #ifdef SC_FEATURE_SENTRY
        #ifdef NDEBUG
            #pragma message("[EON] Using Sentry.") // Display a message during compilation indicating the use of Sentry
            sc::sentry::initialize(SC_SENTRY_DSN, fmt::format("{}@{}", pystring::lower(VER_APP_NAME), VER_APP_VER).data());
            DEFER(sc::sentry::shutdown()); // Register the sentry shutdown function for deferred execution
        #else
        #pragma message("[EON] Sentry is disabled due for debug build.")
        #endif
    #endif

    spdlog::set_default_logger(spdlog::stdout_color_mt(pystring::lower(VER_APP_NAME))); // Set the default logger for spdlog
    spdlog::set_level(spdlog::level::debug); // Set the log level to debug

    #ifndef NDEBUG
        spdlog::critical("This application has been built in DEBUG mode!"); // Log a critical message
    #endif

    spdlog::info("Program version: {}", VER_APP_VER); // Log an info message

    if (const auto err = _sc_bootstrap(_sc_run); err.has_value()) {
        spdlog::error("An error has occurred: {}", *err); // Log an error message

        std::stringstream ss;
        ss << "This program experienced an error and was unable to continue running:";
        ss << std::endl << std::endl;
        ss << err.value().data();

        #ifndef SC_FEATURE_NO_ERROR_MESSAGE
            MessageBoxA(NULL, ss.str().data(), fmt::format("{} Error", VER_APP_NAME).data(), MB_OK | MB_ICONERROR); // Display an error message box
        #endif

        return 1; // Return 1 to indicate an error
    }

    return 0; // Return 0 to indicate success
}

int main(int arg_c, char **arg_v) {
    _sc_print_publisher_info(arg_c, arg_v); // Print publisher information

    return _sc_entry_point(arg_c, arg_v); // Call the entry point function
}
