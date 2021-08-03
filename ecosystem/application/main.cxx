#include <optional>
#include <vector>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>
#include <glbinding/glbinding.h>
#include <glbinding/gl33core/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>

#include "application.h"

#include "../common/imgui_impl_glfw.h"
#include "../common/imgui_impl_opengl3.h"
#include "../common/imgui_freetype.h"
#include "../common/imgui_utils.hpp"
#include "../common/defer.h"
#include "../common/font_awesome_5.h"
#include "../common/font_awesome_5_brands.h"
#include "../common/hid.h"

using namespace gl;

static auto scGetProcAddress(const char *name) {
    const auto addr = glfwGetProcAddress(name);
    const auto text = fmt::format("GL: {} @ {}", name, reinterpret_cast<void *>(addr));
    if (addr) spdlog::debug(text);
    else {
        spdlog::error(text);
        exit(1000);
    }
    return addr;
}

static std::optional<glm::ivec2> scGetWindowCenter(GLFWwindow *const window) {
    int x, y, w, h;
    glfwGetWindowPos(window, &x, &y);
    if (glfwGetError(nullptr) != GLFW_NO_ERROR) return std::nullopt;
    glfwGetWindowSize(window, &w, &h);
    if (glfwGetError(nullptr) != GLFW_NO_ERROR) return std::nullopt;
    return glm::ivec2(x + (w / 2), y + (h / 2));
}

static std::optional<int> scGetWindowDisplayHz(GLFWwindow *const window) {
    if (const auto center = scGetWindowCenter(window); center) {
        int num_monitors;
        const auto monitors = glfwGetMonitors(&num_monitors);
        for (int i = 0; i < num_monitors; i++) {
            const auto this_monitor = monitors[i];
            int x, y, w, h;
            glfwGetMonitorWorkarea(this_monitor, &x, &y, &w, &h);
            if (center->x >= x && center->x < x + w && center->y >= y && center->y < y + h) {
                const auto this_monitor_vm = glfwGetVideoMode(this_monitor);
                if (this_monitor_vm) return this_monitor_vm->refreshRate;
                break;
            }
        }
    }
    if (const auto primary_monitor = glfwGetPrimaryMonitor(); primary_monitor) {
        if (const auto primary_vm = glfwGetVideoMode(primary_monitor); primary_vm) {
            return primary_vm->refreshRate;
        }
    }
    return std::nullopt;
}

int main() {
    if (const auto devices = sc::hid::list_devices(); devices) {
        for (auto &e : *devices) {
            spdlog::info("{} -> {}", e.instance_path, e.product_name);
        }
    }
    const int ifbw = 932, ifbh = 768;
    DEFER({
        spdlog::debug("Terminating GLFW...");
        glfwTerminate();
    });
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Hello, World!");
    if (glfwInit() != GLFW_TRUE) return 1;
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    const auto window = glfwCreateWindow(ifbw, ifbh, "Visor", nullptr, nullptr);
    if (!window) return 2;
    DEFER({
        spdlog::debug("Destroying main window...");
        glfwDestroyWindow(window);
    });
    glfwMakeContextCurrent(window);
    glbinding::initialize(scGetProcAddress, false);
    spdlog::info("GL: {} ({})", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    const auto imgui_ctx = ImGui::CreateContext();
    DEFER({
        spdlog::debug("Destroying ImGui context...");
        ImGui::DestroyContext(imgui_ctx);
    });
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    DEFER({
        spdlog::debug("Shutting down ImGui GLFW...");
        ImGui_ImplGlfw_Shutdown();
    });
    ImGui_ImplOpenGL3_Init("#version 130");
    DEFER({
        spdlog::debug("Shutting down ImGui OpenGL3...");
        ImGui_ImplOpenGL3_Shutdown();
    });
    {
        ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", 16);
        {
            static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
            ImFontConfig icons_config;
            icons_config.MergeMode = true;
            icons_config.PixelSnapH = true;
            ImGui::GetIO().Fonts->AddFontFromFileTTF( FONT_ICON_FILE_NAME_FAS, 12, &icons_config, icons_ranges);
        }
        {
            static const ImWchar icons_ranges[] = { ICON_MIN_FAB, ICON_MAX_FAB, 0 };
            ImFontConfig icons_config;
            icons_config.MergeMode = true;
            icons_config.PixelSnapH = true;
            ImGui::GetIO().Fonts->AddFontFromFileTTF( FONT_ICON_FILE_NAME_FAB, 12, &icons_config, icons_ranges);
        }
        auto &style = ImGui::GetStyle();
        style.WindowBorderSize = 1;
        style.FrameBorderSize = 1;
        style.FrameRounding = 3.f;
        style.ChildRounding = 3.f;
        style.ScrollbarRounding = 3.f;
        style.WindowRounding = 3.f;
    }
    ImDrawCompare drawCmp;
    ImFreetypeEnablement freetype;
    int rfbw = 0, rfbh = 0;
    while (!glfwWindowShouldClose(window)) {
        const auto hz = scGetWindowDisplayHz(window);
        static auto oldHz = hz;
        if (hz && hz && *hz != *oldHz) spdlog::debug("Hz: {} -> {}", *oldHz, *hz);
        oldHz = hz;
        glfwPollEvents();
        const auto [ cfbw, cfbh ] = [&]() -> std::tuple<int, int> {
            int dw, dh;
            glfwGetFramebufferSize(window, &dw, &dh);
            return { dw, dh };
        }();
        freetype.preNewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        Application::emitUserInterface(cfbw, cfbh);
        ImGui::Render();
        const auto drawData = ImGui::GetDrawData();
        const bool drawDataChanged = !drawCmp.Check(drawData);
        const bool fbSizeChanged = (cfbw != rfbw || cfbh != rfbh);
        const bool needRedraw = fbSizeChanged || drawDataChanged;
        if (!needRedraw) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / (hz ? *hz : 60)));
            continue;
        }
        if (fbSizeChanged) {
            spdlog::info("Framebuffer: {} -> {}, {} -> {}", rfbw, cfbw, rfbh, cfbh);
            glViewport(0, 0, cfbw, cfbh);
            rfbw = cfbw;
            rfbh = cfbh;
        }
        spdlog::info("Rendering.");
        glClearColor(.4, .6, .8, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(drawData);
        glfwSwapBuffers(window);
    }
    return 0;
}