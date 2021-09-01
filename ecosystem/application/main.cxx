#define WIN32_LEAN_AND_MEAN
#define SDL_MAIN_HANDLED

#include <windows.h>

#undef min
#undef max

#include <algorithm>
#include <optional>
#include <vector>
#include <string_view>
#include <sstream>
#include <functional>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <SDL2/SDL.h>
#include <glbinding/glbinding.h>
#include <glbinding/gl33core/gl.h>
#include <imgui.h>

#include "application.h"

#include "../imgui/imgui_impl_sdl.h"
#include "../imgui/imgui_impl_opengl3.h"
#include "../imgui/imgui_freetype.h"
#include "../imgui/imgui_utils.hpp"
#include "../defer.hpp"
#include "../expected.hpp"
#include "../hidhide/hidhide.h"
#include "../boot/gl-proc-address.h"
#include "../font/imgui.h"
#include "../systray/systray.h"

#include "../vigem/Client.h"

using namespace gl;

static std::optional<std::string> prepare_styling() {
    if (!sc::font::imgui::load(16)) return "Failed to load fonts.";
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

std::optional<std::string> bootstrap(std::function<std::optional<std::string>(SDL_Window *, ImGuiContext *, PVIGEM_CLIENT, PVIGEM_TARGET)> success_cb) {
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
    const auto sdl_window = SDL_CreateWindow("Visor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, initial_framebuffer_size.x, initial_framebuffer_size.y, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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
    if (const auto styling_err = prepare_styling(); styling_err.has_value()) return styling_err;
    const auto vigem_client = vigem_alloc();
    if (vigem_client == nullptr) return "Failed to allocate memory for ViGEm.";
    spdlog::debug("ViGEM client: {}", reinterpret_cast<void *>(vigem_client));
    DEFER({
        spdlog::debug("Freeing ViGEm client...");
        vigem_free(vigem_client);
    });
    const auto vigem_connect_status = vigem_connect(vigem_client);
    if (!VIGEM_SUCCESS(vigem_connect_status)) return "Failed to connect to ViGEm.";
    DEFER({
        spdlog::debug("Disconnecting ViGEm client...");
        vigem_disconnect(vigem_client);
    });
    const auto vigem_pad = vigem_target_ds4_alloc();
    if (vigem_pad == nullptr) return "Failed to allocate memory for ViGEm gamepad.";
    vigem_target_set_vid(vigem_pad, 1001);
    vigem_target_set_pid(vigem_pad, 2003);
    DEFER({
        spdlog::debug("Freeing ViGEm gamepad...");
        vigem_target_free(vigem_pad);
    });
    if (!VIGEM_SUCCESS(vigem_target_add(vigem_client, vigem_pad))) return "Failed to initialize ViGEm gamepad.";
    DEFER({
        spdlog::debug("Unplugging ViGEm gamepad...");
        vigem_target_remove(vigem_client, vigem_pad);
    });
    spdlog::info("Bootstrapping completed.");
    if (const auto cb_err = success_cb(sdl_window, imgui_ctx, vigem_client, vigem_pad); cb_err.has_value()) {
        spdlog::warn("Bootstrap success routine returned an error: {}", *cb_err);
        return cb_err;
    }
    return std::nullopt;
}

static void process_joystick_events(const SDL_Event &sdl_event) {
    if (sdl_event.type == SDL_JOYAXISMOTION) {
        // spdlog::info("JOY axis: {}, {}, {}", event.jaxis.which, event.jaxis.axis, static_cast<int>(event.jaxis.value) + 32768);
    } else if (sdl_event.type == SDL_JOYHATMOTION) {
        spdlog::debug("JOY hat: {}, {}, {}", sdl_event.jhat.which, sdl_event.jhat.hat, sdl_event.jhat.value);
    } else if (sdl_event.type == SDL_JOYBUTTONDOWN) {
        spdlog::debug("JOY button: {}, {}, {}", sdl_event.jbutton.which, sdl_event.jbutton.button, sdl_event.jbutton.state);
    } else if (sdl_event.type == SDL_JOYDEVICEADDED) {
        const auto name = SDL_JoystickNameForIndex(sdl_event.jdevice.which);
        spdlog::debug("JOY added: #{}, \"{}\"", sdl_event.jdevice.which, name);
        const auto num_joysticks = SDL_NumJoysticks();
        for (int i = 0; i < num_joysticks; i++) {
            if (const auto joystick = SDL_JoystickOpen(i); joystick) {
                const auto instance_id = SDL_JoystickInstanceID(joystick);
                if (std::find(sc::visor::joysticks.begin(), sc::visor::joysticks.end(), instance_id) == sc::visor::joysticks.end()) sc::visor::joysticks.push_back(instance_id);
            }
        }
    } else if (sdl_event.type == SDL_JOYDEVICEREMOVED) {
        spdlog::debug("JOY removed: #{}", sdl_event.jdevice.which);
        const auto i = std::find(sc::visor::joysticks.begin(), sc::visor::joysticks.end(), sdl_event.jdevice.which);
        if (i != sc::visor::joysticks.end()) sc::visor::joysticks.erase(i);
    }
}

static bool process_events(SDL_Window *sdl_window) {
    bool should_quit = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        process_joystick_events(event);
        if (event.type == SDL_QUIT) {
            spdlog::debug("Got quit event.");
            should_quit = true;
        }
    }
    if (should_quit) {
        SDL_HideWindow(sdl_window);
        return false;
    }
    return true;
}

static std::optional<std::string> run(SDL_Window *sdl_window, ImGuiContext *imgui_ctx, PVIGEM_CLIENT vigem_client, PVIGEM_TARGET vigem_pad) {
    sc::systray::enable([sdl_window]() {
        SDL_ShowWindow(sdl_window);
        SDL_RestoreWindow(sdl_window);
    });
    DEFER(sc::systray::disable());
    ImDrawCompare im_draw_cache;
    ImFreetypeEnablement freetype;
    glm::ivec2 recent_framebuffer_size { 0, 0 };
    SDL_JoystickEventState(SDL_ENABLE);
    /*
    XUSB_REPORT report;
    report.bLeftTrigger = 0;
    report.bRightTrigger = 0;
    report.sThumbLX = 0;
    report.sThumbLY = 0;
    report.sThumbRX = 0;
    report.sThumbRY = 0;
    report.wButtons = 0;
    */
    for (;;) {
        /*
        report.bLeftTrigger = rand() % 255;
        report.bRightTrigger = rand() % 255;
        report.sThumbLX = -10000 + (rand() % 20000);
        report.sThumbLY = -10000 + (rand() % 20000);
        report.sThumbRX = -10000 + (rand() % 20000);
        report.sThumbRY = -10000 + (rand() % 20000);
        if (!VIGEM_SUCCESS(vigem_target_x360_update(vigem_client, vigem_pad, report))) return "Failed to update ViGEm gamepad.";
        fmt::print("{}, {}\n", report.bLeftTrigger, report.bRightTrigger);
        */
        if (!process_events(sdl_window)) break;
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
        freetype.preNewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        sc::visor::emit_ui(current_framebuffer_size);
        ImGui::Render();
        const auto draw_data = ImGui::GetDrawData();
        const bool draw_data_changed = !im_draw_cache.Check(draw_data);
        const bool framebuffer_size_changed = (current_framebuffer_size.x != recent_framebuffer_size.x || current_framebuffer_size.y != recent_framebuffer_size.y);
        const bool need_redraw = framebuffer_size_changed || draw_data_changed;
        if (!need_redraw) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / (hz ? *hz : 60)));
            continue;
        }
        if (framebuffer_size_changed) {
            spdlog::debug("Framebuffer resized: {} -> {}, {} -> {}", recent_framebuffer_size.x, current_framebuffer_size.x, recent_framebuffer_size.y, current_framebuffer_size.y);
            glViewport(0, 0, current_framebuffer_size.x, current_framebuffer_size.y);
            recent_framebuffer_size.x = current_framebuffer_size.x;
            recent_framebuffer_size.y = current_framebuffer_size.y;
        }
        glClearColor(.4, .6, .8, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        if (static bool shown_window = false; !shown_window) {
            spdlog::debug("First render complete. Making window visible.");
            SDL_ShowWindow(sdl_window);
            shown_window = true;
        }
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
        SDL_GL_SwapWindow(sdl_window);
    }
    return std::nullopt;
}

#include "../sentry/sentry.h"

int main() {
    sc::sentry::initialize("https://f4a284ccd2194db2982e121f4c3f8e1b@o881067.ingest.sentry.io/5942037");
    DEFER(sc::sentry::shutdown());
    spdlog::set_default_logger(spdlog::stdout_color_mt("visor"));
    spdlog::set_level(spdlog::level::debug);
    if (const auto err = bootstrap(run); err.has_value()) {
        spdlog::error("An error has occurred: {}", *err);
        std::stringstream ss;
        ss << "This program experienced an error and was unable to continue running:";
        ss << std::endl << std::endl;
        ss << err.value().data();
        MessageBoxA(NULL, ss.str().data(), "Sim Coaches Visor Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    return 0;
}