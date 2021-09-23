#include <string_view>
#include <array>
#include <vector>
#include <optional>
#include <functional>
#include <imgui.h>
#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/vec2.hpp>

#include "application.h"

#include "../font/font_awesome_5.h"
#include "../font/font_awesome_5_brands.h"
#include "../imgui/imgui_utils.hpp"

namespace sc::visor {

    enum class view {
        device_panel,
        notify_saved_panel,
    };

    static view current_view = view::device_panel;

    template<typename A, typename B, typename C> B xy_as(const A &in) {
        return B {
            static_cast<C>(in.x),
            static_cast<C>(in.y)
        };
    }

    #define IM_GLMD2(v) xy_as<ImVec2, glm::dvec2, double>(v)
    #define GLMD_IM2(v) xy_as<glm::dvec2, ImVec2, float>(v) 

    static glm::dvec2 coords_to_screen(const glm::dvec2 &in, const glm::dvec2 &min, const glm::dvec2 &size) {
        return {
            (in.x * size.x) + min.x,
            (in.y * -size.y) + (min.y + size.y)
        };
    }

    static glm::dvec2 bezier(std::vector<glm::dvec2> inputs, double power, std::optional<std::function<void(const std::vector<glm::dvec2> &level)>> callback = std::nullopt) {
        for (;;) {
            for (int i = 0; i < inputs.size() - 1; i++) inputs[i] = glm::mix(inputs[i], inputs[i + 1], power);
            inputs.resize(inputs.size() - 1);
            if (inputs.size() == 1) return inputs.front();
            if (callback) (*callback)(inputs);
        }
    }

    static void cubic_bezier_plot(const std::vector<glm::dvec2> &inputs, const glm::ivec2 &size, std::optional<double> fraction = std::nullopt, std::optional<double> limit_min = std::nullopt, std::optional<double> limit_max = std::nullopt) {
        auto draw_list = ImGui::GetWindowDrawList();
        auto bez_area_min = ImGui::GetCursorScreenPos();
        auto bez_area_size = size;
        ImGui::Dummy(GLMD_IM2(size));
        draw_list->AddRectFilled(
            { static_cast<float>(bez_area_min.x), static_cast<float>(bez_area_min.y) },
            { static_cast<float>(bez_area_min.x + bez_area_size.x), static_cast<float>(bez_area_min.y + bez_area_size.y) },
            IM_COL32(255, 255, 255, 32),
            ImGui::GetStyle().FrameRounding
        );
        ImGui::PushClipRect(bez_area_min, { bez_area_min.x + bez_area_size.x, bez_area_min.y + bez_area_size.y }, false);
        bez_area_min.x += 12;
        bez_area_min.y += 12;
        bez_area_size.x -= 24;
        bez_area_size.y -= 24;
        auto screen_p = inputs;
        for (auto &sp : screen_p) sp = coords_to_screen(sp, IM_GLMD2(bez_area_min), bez_area_size);
        for (int i = 1; i < inputs.size(); i++) draw_list->AddLine(GLMD_IM2(screen_p[i - 1]), GLMD_IM2(screen_p[i]), IM_COL32(128, 255, 128, 32), 2.f);
        const int num_curve_segments = 30;
        auto last_plot = GLMD_IM2(coords_to_screen(inputs[0], IM_GLMD2(bez_area_min), bez_area_size));
        for (int i = 1; i < num_curve_segments; i++) {
            const double power = (1.0 / static_cast<double>(num_curve_segments)) * static_cast<double>(i);
            const auto here = GLMD_IM2(coords_to_screen(bezier(inputs, power), IM_GLMD2(bez_area_min), bez_area_size));
            draw_list->AddLine(last_plot, here, IM_COL32(255, 165, 0, 255), 2.f);
            last_plot = here;
        }
        draw_list->AddLine(last_plot, GLMD_IM2(coords_to_screen(inputs.back(), IM_GLMD2(bez_area_min), bez_area_size)), IM_COL32(255, 165, 0, 255), 2.f);
        std::optional<int> hovering_point_i;
        for (int i = 0; i < inputs.size(); i++) {
            auto color = (i == 0 || i == inputs.size() - 1) ? IM_COL32(255, 165, 0, 255) : IM_COL32(255, 255, 255, 128);
            ImGui::SetCursorScreenPos(GLMD_IM2(screen_p[i] - 3.0));
            if (!hovering_point_i && ImGui::IsMouseHoveringRect(GLMD_IM2(screen_p[i] - 3.0), GLMD_IM2(screen_p[i] + 3.0))) {
                ImGui::BeginTooltip();
                ImGui::Text(fmt::format("#{}: x{}, y{}", i + 1, inputs[i].x, inputs[i].y).data());
                ImGui::EndTooltip();
                draw_list->AddRect(GLMD_IM2(screen_p[i] - 6.0), GLMD_IM2(screen_p[i] + 7.0), color, ImGui::GetStyle().FrameRounding, 0, 2);
                hovering_point_i = i;
            } else draw_list->AddRect(GLMD_IM2(screen_p[i] - 3.0), GLMD_IM2(screen_p[i] + 4.0), color, ImGui::GetStyle().FrameRounding, 0, 2);
        }
        {
            const auto top_left = GLMD_IM2(coords_to_screen({ 0, inputs.back().y }, IM_GLMD2(bez_area_min), bez_area_size));
            const auto top_right = GLMD_IM2(coords_to_screen(inputs.back(), IM_GLMD2(bez_area_min), bez_area_size));
            draw_list->AddLine(top_left, top_right, IM_COL32(255, 255, 255, 200), 2.f);
            draw_list->AddText({ top_left.x, top_left.y + 2 }, IM_COL32(255, 255, 255, 128), fmt::format("{}%", static_cast<int>(inputs.back().y * 100.0)).data());
        }
        {
            const auto bottom_right = GLMD_IM2(coords_to_screen({ 1, inputs.front().y }, IM_GLMD2(bez_area_min), bez_area_size));
            const auto bottom_left = GLMD_IM2(coords_to_screen(inputs.front(), IM_GLMD2(bez_area_min), bez_area_size));
            draw_list->AddLine(bottom_left, bottom_right, IM_COL32(255, 255, 255, 200), 2.f);
            const auto text = fmt::format("{}%", static_cast<int>(inputs.front().y * 100.0));
            const auto text_dim = ImGui::CalcTextSize(text.data());
            draw_list->AddText({ bottom_right.x - text_dim.x, bottom_right.y - text_dim.y - 2 }, IM_COL32(255, 255, 255, 128), text.data());
        }
        bez_area_min.x -= 12;
        bez_area_min.y -= 12;
        bez_area_size.x += 24;
        bez_area_size.y += 24;
        ImGui::PopClipRect();
        ImGui::SetCursorScreenPos({ bez_area_min.x, bez_area_min.y + bez_area_size.y + ImGui::GetStyle().FramePadding.y });
    }

    static void emit_content_notify_saved_panel() {
        ImPenUtility pen;
        pen.CalculateWindowBounds();
        ImGui::SetCursorScreenPos(pen.GetCenteredPosition({ 300, 150 }));
        if (ImGui::BeginChild("##NotifySavedWindow", { 300, 150 }, true, ImGuiWindowFlags_MenuBar)) {
            if (ImGui::BeginMenuBar()) {
                ImGui::Text(fmt::format("Saved", ICON_FA_SITEMAP).data());
                ImGui::EndMenuBar();
            }
            ImGui::Text("All data has been saved.");
            if (ImGui::Button("Okay")) current_view = view::device_panel;
        }
        ImGui::EndChild();
    }

    static void emit_content_panel() {
        switch (current_view) {
            case view::device_panel:

                break;
            case view::notify_saved_panel:
                emit_content_notify_saved_panel();
                break;
        }
    }

    static void emit_primary_window_menu_bar() {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu(fmt::format("{} File", ICON_FA_FILE_CODE).data())) {
                if (ImGui::Selectable(fmt::format("{} Save", ICON_FA_SAVE).data())) current_view = view::notify_saved_panel;
                if (ImGui::Selectable(fmt::format("{} Quit", ICON_FA_SKULL).data())) keep_running = false;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(fmt::format("{} Devices", ICON_FA_CUBES).data())) {
                ImGui::TextDisabled("No devices.");
                ImGui::Selectable(fmt::format("{} Clear System Calibrations", ICON_FA_ERASER).data());
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(fmt::format("{} Help", ICON_FA_QUESTION_CIRCLE).data())) {
                ImGui::Selectable(fmt::format("{} Report Bug", ICON_FA_BUG).data());
                ImGui::Selectable(fmt::format("{} Make Comment", ICON_FA_COMMENT_ALT).data());
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }
}

void sc::visor::emit_ui(const glm::ivec2 &framebuffer_size) {
    ImGui::SetNextWindowPos({ 0, 0 }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ static_cast<float>(framebuffer_size.x), static_cast<float>(framebuffer_size.y) }, ImGuiCond_Always);
    if (ImGui::Begin("##PrimaryWindow", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        emit_primary_window_menu_bar();
        emit_content_panel();
    }
    ImGui::End();
}