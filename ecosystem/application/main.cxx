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
#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <SDL2/SDL.h>
#include <glbinding/glbinding.h>
#include <glbinding/gl33core/gl.h>
#include <imgui.h>

#include "application.h"

#include "../storage/storage.h"
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

XUSB_REPORT *gamepad = nullptr;

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

static std::optional<std::filesystem::path> get_module_file_path() {
    TCHAR path[MAX_PATH];
    if (GetModuleFileNameA(NULL, path, sizeof(path)) == 0) return std::nullopt;
    return path;
}

static std::optional<std::string> module_to_whitelist() {
    const auto this_exe = get_module_file_path();
    if (!this_exe) return "Unable to assess module file path.";
    if (!sc::hidhide::set_whitelist({ *this_exe })) return "Unable to update HIDHIDE whitelist.";
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
    const auto vigem_pad = vigem_target_x360_alloc();
    if (vigem_pad == nullptr) return "Failed to allocate memory for ViGEm gamepad.";
    DEFER({
        spdlog::debug("Freeing ViGEm gamepad...");
        vigem_target_free(vigem_pad);
    });
    if (!VIGEM_SUCCESS(vigem_target_add(vigem_client, vigem_pad))) return "Failed to initialize ViGEm gamepad.";
    DEFER({
        spdlog::debug("Unplugging ViGEm gamepad...");
        vigem_target_remove(vigem_client, vigem_pad);
    });
    if (const auto error = module_to_whitelist(); error) return "Unable to add this module to HIDHIDE whitelist.";
    spdlog::info("Bootstrapping completed.");
    if (const auto cb_err = success_cb(sdl_window, imgui_ctx, vigem_client, vigem_pad); cb_err.has_value()) {
        spdlog::warn("Bootstrap success routine returned an error: {}", *cb_err);
        return cb_err;
    }
    return std::nullopt;
}

static bool hide_product(const std::string_view &name) {
    if (!sc::hidhide::is_enabled()) {
        if (!sc::hidhide::set_enabled(true)) {
            spdlog::error("Unable to activate HIDHIDE.");
            return false;
        } else spdlog::debug("Activated HIDHIDE as it was disabled.");
    } // else spdlog::debug("HIDHIDE is already enabled.");
    const auto present_devices = sc::hidhide::list_devices();
    if (!present_devices) {
        spdlog::error("Unable to enumerate hidden devices.");
        return false;
    }
    auto blacklist = sc::hidhide::get_blacklist();
    if (!blacklist) {
        spdlog::error("Unable to get HIDHIDE blacklist");
        return false;
    }
    bool blacklist_changed = false;
    for (auto device : *present_devices) {
        if (name != device.product_name) continue;
        if (std::find(blacklist->begin(), blacklist->end(), device.instance_path) != blacklist->end()) continue;
        spdlog::debug("Hiding: {} @ {}", device.product_name, device.instance_path);
        blacklist->push_back(device.instance_path);
        blacklist_changed = true;
    }
    if (!blacklist_changed) {
        spdlog::debug("No new devices to hide.");
        return true;
    }
    if (!sc::hidhide::set_blacklist(*blacklist)) {
        spdlog::error("Unable to update hidden devices listing.");
        return false;
    }
    spdlog::debug("HIDHIDE list updated.");
    return true;
}

static void process_joystick_events(const SDL_Event &sdl_event) {
    if (sdl_event.type == SDL_JOYAXISMOTION) {
        const auto vid = SDL_JoystickGetVendor(SDL_JoystickFromInstanceID(sdl_event.jaxis.which));
        const auto pid = SDL_JoystickGetProduct(SDL_JoystickFromInstanceID(sdl_event.jaxis.which));
        if (!(vid == 7634 && pid == 10037)) return;
        if (auto i = std::find_if(sc::visor::joysticks.begin(), sc::visor::joysticks.end(), [&](std::shared_ptr<sc::visor::joystick> joy) {
            return joy->instance_id == sdl_event.jaxis.which;
        }); i != sc::visor::joysticks.end()) {
            i->get()->axes[sdl_event.jaxis.axis].val = sdl_event.jaxis.value;
            i->get()->axes[sdl_event.jaxis.axis].fraction = glm::clamp((static_cast<float>(i->get()->axes[sdl_event.jaxis.axis].val) - static_cast<float>(i->get()->axes[sdl_event.jaxis.axis].min)) / (static_cast<float>(i->get()->axes[sdl_event.jaxis.axis].max) - static_cast<float>(i->get()->axes[sdl_event.jaxis.axis].min)), 0.f, 1.f);
            switch (sdl_event.jaxis.axis) {
                case 0:
                    gamepad->bLeftTrigger = glm::clamp(static_cast<BYTE>(i->get()->axes[sdl_event.jaxis.axis].fraction * static_cast<float>(std::numeric_limits<BYTE>::max())), std::numeric_limits<BYTE>::min(), std::numeric_limits<BYTE>::max());
                    // gamepad->bTriggerL = glm::clamp(static_cast<BYTE>(i->get()->axes[sdl_event.jaxis.axis].fraction * static_cast<float>(std::numeric_limits<BYTE>::max())), static_cast<BYTE>(0), std::numeric_limits<BYTE>::max());
                    // spdlog::info("bTriggerL {}", gamepad->bTriggerL);
                    break;
                case 1:
                    // gamepad->bTriggerR = glm::clamp(static_cast<BYTE>(i->get()->axes[sdl_event.jaxis.axis].fraction * static_cast<float>(std::numeric_limits<BYTE>::max())), static_cast<BYTE>(0), std::numeric_limits<BYTE>::max());
                    // spdlog::info("bTriggerR {}", gamepad->bTriggerR);
                    break;
                case 2:
                    // gamepad->bThumbLY = 128 + glm::clamp(static_cast<BYTE>(i->get()->axes[sdl_event.jaxis.axis].fraction * static_cast<float>(127)), static_cast<BYTE>(0), static_cast<BYTE>(127));
                    // spdlog::info("bThumbLY {}", gamepad->bThumbLY);
                    break;
            }
        } else spdlog::warn("BAD");
    } else if (sdl_event.type == SDL_JOYHATMOTION) {
        spdlog::debug("Joystick hat: {}, {}, {}", sdl_event.jhat.which, sdl_event.jhat.hat, sdl_event.jhat.value);
    } else if (sdl_event.type == SDL_JOYBUTTONDOWN) {
        spdlog::debug("Joystick button: {}, {}, {}", sdl_event.jbutton.which, sdl_event.jbutton.button, sdl_event.jbutton.state);
    } else if (sdl_event.type == SDL_JOYDEVICEADDED) {
        const auto num_joysticks = SDL_NumJoysticks();
        for (int i = 0; i < num_joysticks; i++) {
            if (const auto joystick = SDL_JoystickOpen(i); joystick) {
                const auto instance_id = SDL_JoystickInstanceID(joystick);
                const auto name = SDL_JoystickName(joystick);
                if (strcmp("Sim Coaches P1 Pro Pedals", name) != 0) continue;
                hide_product(name);
                if (std::find_if(sc::visor::joysticks.begin(), sc::visor::joysticks.end(), [&](std::shared_ptr<sc::visor::joystick> &joy) {
                    return joy->instance_id == instance_id;
                }) == sc::visor::joysticks.end()) {
                    auto new_joystick = std::make_shared<sc::visor::joystick>(instance_id);
                    const auto num_axes = SDL_JoystickNumAxes(joystick);
                    const auto num_buttons = 0; // SDL_JoystickNumButtons(joystick);
                    const auto num_hats = 0; // SDL_JoystickNumHats(joystick);
                    if (num_axes < 0 || num_buttons < 0 || num_hats < 0) {
                        spdlog::error("Unable to enumerate joystick: {}, {} axes, {} buttons, {} hats", name, num_axes, num_buttons, num_hats);
                        continue;
                    }
                    new_joystick->axes.resize(num_axes);
                    new_joystick->buttons.resize(num_buttons);
                    new_joystick->hats.resize(num_hats);
                    sc::visor::joysticks.push_back(new_joystick);
                    spdlog::debug("Joystick added: #{}, \"{}\", {} axes, {} buttons, {} hats", sdl_event.jdevice.which, name, num_axes, num_buttons, num_hats);
                }
            }
        }
    } else if (sdl_event.type == SDL_JOYDEVICEREMOVED) {
        spdlog::debug("Joystick removed: #{}", sdl_event.jdevice.which);
        const auto i = std::find_if(sc::visor::joysticks.begin(), sc::visor::joysticks.end(), [&](std::shared_ptr<sc::visor::joystick> &joy) {
            return joy->instance_id == sdl_event.jdevice.which;
        });
        if (i != sc::visor::joysticks.end()) sc::visor::joysticks.erase(i);
    }
}

static bool process_events(SDL_Window *sdl_window) {
    bool should_quit = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
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

#include "../storage/storage-object.h"

static std::optional<std::string> run(SDL_Window *sdl_window, ImGuiContext *imgui_ctx, PVIGEM_CLIENT vigem_client, PVIGEM_TARGET vigem_pad) {
    sc::systray::enable([sdl_window]() {
        SDL_ShowWindow(sdl_window);
        SDL_RestoreWindow(sdl_window);
    });
    DEFER(sc::systray::disable());
    if (const auto err = sc::storage::initialize(); err) return err;
    DEFER(sc::storage::shutdown());
    sc::storage::set_flag("accepted-eula", true);
    sc::storage::set_flag("walkthrough-completed", true);
    {
        YAML::Emitter out;
        out.SetIndent(2);
        out << YAML::BeginMap;
        out << YAML::Key << "username" << YAML::Value << "brandon@simcoaches.com";
        out << YAML::Key << "password_hash" << YAML::Value << "123abc";
        out << YAML::Key << "nanner" << YAML::Value;
            out << YAML::BeginSeq;
            out << "This is a test!";
            out << "This is another test!";
            out << YAML::EndSeq;
        out << YAML::EndMap;
        sc::storage::set_object("user-credentials", out);
    }
    sc::storage::sync();
    ImDrawCompare im_draw_cache;
    ImFreetypeEnablement freetype;
    glm::ivec2 recent_framebuffer_size { 0, 0 };
    SDL_JoystickEventState(SDL_ENABLE);
    XUSB_REPORT gamepad_report;
    XUSB_REPORT_INIT(&gamepad_report);
    gamepad = &gamepad_report;
    for (;;) {
        if (!process_events(sdl_window)) break;
        if (!VIGEM_SUCCESS(vigem_target_x360_update(vigem_client, vigem_pad, gamepad_report))) return "Failed to update ViGEm gamepad.";
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
        freetype.PreNewFrame();
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

#include <pystring.h>

int main() {
    #ifdef NDEBUG
    sc::sentry::initialize("https://f4a284ccd2194db2982e121f4c3f8e1b@o881067.ingest.sentry.io/5942037", fmt::format("{}@{}", pystring::lower(SC_APP_NAME), SC_APP_VER).data());
    DEFER(sc::sentry::shutdown());
    #endif
    spdlog::set_default_logger(spdlog::stdout_color_mt("visor"));
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
        MessageBoxA(NULL, ss.str().data(), "Sim Coaches Visor Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    return 0;
}