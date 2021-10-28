#include "bezier.h"
#include "im_glm_vec.hpp"

#include <glm/common.hpp>
#include <fmt/format.h>

namespace sc::bezier::ui {

    static glm::dvec2 coords_to_screen(const glm::dvec2 &in, const glm::dvec2 &min, const glm::dvec2 &size) {
        return {
            (in.x * size.x) + min.x,
            (in.y * -size.y) + (min.y + size.y)
        };
    }
}

glm::dvec2 sc::bezier::calculate(std::vector<glm::dvec2> inputs, double power, std::optional<std::function<void(const std::vector<glm::dvec2> &level)>> callback) {
    for (;;) {
        for (int i = 0; i < inputs.size() - 1; i++) inputs[i] = glm::mix(inputs[i], inputs[i + 1], power);
        inputs.resize(inputs.size() - 1);
        if (inputs.size() == 1) return inputs.front();
        if (callback) (*callback)(inputs);
    }
}

void sc::bezier::ui::plot_cubic(std::vector<glm::dvec2> inputs, const glm::ivec2 &size, std::optional<double> fraction, std::optional<double> limit_min, std::optional<double> limit_max, std::optional<double> fraction_h) {
    if (limit_min) for (auto &p : inputs) p.y += *limit_min * (1.0 - p.y);
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
    ImGui::PushClipRect(bez_area_min, { bez_area_min.x + bez_area_size.x, bez_area_min.y + bez_area_size.y }, true);
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
        const auto here = GLMD_IM2(coords_to_screen(calculate(inputs, power), IM_GLMD2(bez_area_min), bez_area_size));
        draw_list->AddCircleFilled(last_plot, 1.f, IM_COL32(255, 165, 0, 255));
        draw_list->AddLine(last_plot, here, IM_COL32(255, 165, 0, 255), 2.f);
        last_plot = here;
    }
    draw_list->AddLine(last_plot, GLMD_IM2(coords_to_screen(inputs.back(), IM_GLMD2(bez_area_min), bez_area_size)), IM_COL32(255, 165, 0, 255), 2.f);
    if (fraction.has_value()) {
        const float height = bez_area_size.y * *fraction;
        draw_list->AddLine({ bez_area_min.x, (bez_area_min.y + bez_area_size.y) - height }, { bez_area_min.x + bez_area_size.x, (bez_area_min.y + bez_area_size.y) - height }, IM_COL32(128, 255, 128, 64), 2.f);
    }
    if (fraction_h.has_value()) {
        const float width = bez_area_size.x * *fraction_h;
        draw_list->AddLine({ bez_area_min.x + width, bez_area_min.y }, { bez_area_min.x + width, bez_area_min.y + bez_area_size.y }, IM_COL32(128, 128, 255, 64), 2.f);
    }
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
    if (limit_max) {
        const auto top_left = GLMD_IM2(coords_to_screen({ 0, *limit_max }, IM_GLMD2(bez_area_min), bez_area_size));
        const auto top_right = GLMD_IM2(coords_to_screen({ 0.2, *limit_max }, IM_GLMD2(bez_area_min), bez_area_size));
        draw_list->AddLine(top_left, top_right, IM_COL32(255, 255, 255, 200), 2.f);
        draw_list->AddText({ top_left.x, top_left.y + 2 }, IM_COL32(255, 255, 255, 128), fmt::format("{}%", static_cast<int>(glm::round(*limit_max * 100.0))).data());
    }
    if (limit_min) {
        const auto bottom_right = GLMD_IM2(coords_to_screen({ 1, *limit_min }, IM_GLMD2(bez_area_min), bez_area_size));
        const auto bottom_left = GLMD_IM2(coords_to_screen({ 0, *limit_min }, IM_GLMD2(bez_area_min), bez_area_size));
        draw_list->AddLine(bottom_left, bottom_right, IM_COL32(255, 255, 255, 200), 2.f);
        const auto text = fmt::format("{}%", static_cast<int>(glm::round(*limit_min * 100.0)));
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