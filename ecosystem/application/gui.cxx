#include "gui.h"
#include "application.h"
#include "animation_instance.h"
#include "device_context.h"

#include <string_view>
#include <array>
#include <vector>
#include <optional>
#include <functional>
#include <future>
#include <mutex>
#include <memory>
#include <imgui.h>
#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>
#include <pystring.h>

#include "../font/font_awesome_5.h"
#include "../font/font_awesome_5_brands.h"
#include "../imgui/imgui_utils.hpp"
#include "../defer.hpp"
#include "../resource/resource.h"

namespace sc::visor::gui {

    static animation_instance animation_scan, animation_comm;
    static std::optional<std::chrono::high_resolution_clock::time_point> devices_last_scan;
    static std::future<tl::expected<std::vector<std::shared_ptr<sc::firmware::mk4::device_handle>>, std::string>> devices_future;
    static std::vector<std::shared_ptr<firmware::mk4::device_handle>> devices;
    static std::vector<std::shared_ptr<device_context>> device_contexts;

    static void prepare_styling_colors(ImGuiStyle &style) {
        style.Colors[ImGuiCol_Tab] = { 50.f / 255.f, 50.f / 255.f, 50.f / 255.f, 1.f };
        style.Colors[ImGuiCol_TabActive] = { 70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.f };
        style.Colors[ImGuiCol_TabHovered] = { 90.f / 255.f, 90.f / 255.f, 90.f / 255.f, 1.f };
        style.Colors[ImGuiCol_WindowBg] = { 0x21 / 255.f, 0x25 / 255.f, 0x29 / 255.f, 1.f };
        style.Colors[ImGuiCol_ChildBg] = { 50.f / 255.f, 50.f / 255.f, 50.f / 255.f, 1.f };
        style.Colors[ImGuiCol_FrameBgActive] = { 70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.f };
        style.Colors[ImGuiCol_FrameBgHovered] = { 90.f / 255.f, 90.f / 255.f, 90.f / 255.f, 1.f };
        style.Colors[ImGuiCol_FrameBg] = { 42.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f };
    }

    static void prepare_styling_parameters(ImGuiStyle &style) {
        style.WindowBorderSize = 1;
        style.FrameBorderSize = 1;
        style.FrameRounding = 2.f;
        style.ChildRounding = 2.f;
        style.ScrollbarRounding = 2.f;
        style.WindowRounding = 2.f;
        style.GrabRounding = 2.f;
        style.TabRounding = 2.f;
        style.Colors[ImGuiCol_ChildBg] = { .09f, .09f, .09f, 1.f };
    }

    static std::optional<std::string> prepare_styling() {
        auto &style = ImGui::GetStyle();
        prepare_styling_parameters(style);
        prepare_styling_colors(style);
        return std::nullopt;
    }

    void prepare_animation(const std::string_view &resource_name, animation_instance &instance, const glm::ivec2 &size) {
        if (const auto content = sc::resource::get_resource("DATA", resource_name); content) {
            std::vector<std::byte> buffer(content->second);
            memcpy(buffer.data(), content->first, buffer.size());
            if (const auto sequence = sc::texture::load_lottie_from_memory(resource_name, buffer, size); sequence.has_value()) {
                instance.frame_rate = sequence->frame_rate;
                int frame_i = 0;
                for (const auto &frame : sequence->frames) {
                    const auto description = pystring::lower(fmt::format("<rsc:{}:{}x{}#{}>", resource_name, sequence->frames.front().size.x, sequence->frames.front().size.y, frame_i++));
                    if (const auto texture = sc::texture::upload_to_gpu(frame, sequence->frames.front().size, description); texture.has_value()) {
                        instance.frames.push_back(*texture);
                    } else spdlog::error(texture.error());
                }
            }
        }
    }

    void load_animations() {
        prepare_animation("LOTTIE_LOADING", animation_scan, { 400, 400 });
        prepare_animation("LOTTIE_COMMUNICATING", animation_comm, { 200, 200 });
    }

    static void poll_devices() {
        if (devices_future.valid()) {
            if (devices_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                auto res = devices_future.get();
                DEFER(devices_future = { });
                if (!res.has_value()) {
                    spdlog::error(res.error());
                    return;
                }
                for (auto &new_device : *res) devices.push_back(new_device);
                devices_last_scan = std::chrono::high_resolution_clock::now();
                if (res->size()) spdlog::debug("Found {} devices.", res->size());
            }
        } else {
            if (!devices_last_scan) devices_last_scan = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - *devices_last_scan).count() >= 1) {
                devices_future = async(std::launch::async, []() {
                    return sc::firmware::mk4::discover(devices);
                });
            }
        }
        for (auto &device : devices) {
            const auto contexts_i = std::find_if(device_contexts.begin(), device_contexts.end(), [&device](const std::shared_ptr<device_context> &context) {
                return context->serial == device->serial;
            });
            if (contexts_i != device_contexts.end()) {
                if (contexts_i->get()->handle.get() != device.get()) {
                    spdlog::debug("Applied new handle to device context: {}", device->serial);
                    contexts_i->get()->handle = device;
                    contexts_i->get()->initial_communication_complete = false;
                }
                continue;
            }
            spdlog::debug("Created new device context: {}", device->serial);
            auto new_device_context = std::make_shared<device_context>();
            new_device_context->handle = device;
            new_device_context->name = device->name;
            new_device_context->serial = device->serial;
            device_contexts.push_back(new_device_context);
        }
        for (auto &context : device_contexts) {
            if (!context->handle) continue;
            if (context->update_future.valid()) {
                if (context->update_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    const auto err = context->update_future.get();
                    if (!err.has_value()) continue;
                    spdlog::error("Device context error: {}", *err);
                    devices.erase(std::remove_if(devices.begin(), devices.end(), [&](const std::shared_ptr<firmware::mk4::device_handle> &device) {
                        return device.get() == context->handle.get();
                    }), devices.end());
                    context->initial_communication_complete = false;
                    context->handle.reset();
                } else continue;
            }
            context->update_future = async(std::launch::async, [context]() {
                return device_context::update(context);
            });
        }
    }

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

    static void cubic_bezier_plot(std::vector<glm::dvec2> inputs, const glm::ivec2 &size, std::optional<double> fraction = std::nullopt, std::optional<double> limit_min = std::nullopt, std::optional<double> limit_max = std::nullopt, std::optional<double> fraction_h = std::nullopt) {
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
            const auto here = GLMD_IM2(coords_to_screen(bezier(inputs, power), IM_GLMD2(bez_area_min), bez_area_size));
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
        if (limit_min)
        {
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

    static void emit_axis_profile_slice(const std::shared_ptr<device_context> &context, int axis_i) {
        const auto label_default = axis_i == 0 ? "Throttle" : (axis_i == 1 ? "Brake" : "Clutch");
        if (ImGui::BeginChild(fmt::format("##{}Window", label_default).data(), { 0, 0 }, true, ImGuiWindowFlags_MenuBar)) {
            if (ImGui::BeginMenuBar()) {
                ImGui::Text(fmt::format("{} {} Configurations", ICON_FA_COGS, label_default).data());
                ImGui::EndMenuBar();
            }
            if (ImGui::Button(context->axes[axis_i].enabled ? fmt::format("{} Disable", ICON_FA_PAUSE_CIRCLE).data() : fmt::format("{} Enable", ICON_FA_PLAY_CIRCLE).data(), { ImGui::GetContentRegionAvail().x, 0 })) context->handle->set_axis_enabled(axis_i, !context->axes[axis_i].enabled);
            if (ImGui::BeginChild("##{}InputRangeWindow", { 0, 164 }, true, ImGuiWindowFlags_MenuBar)) {
                bool update_axis_range = false;
                if (ImGui::BeginMenuBar()) {
                    ImGui::Text(fmt::format("{} Range", ICON_FA_RULER).data());
                    ImGui::EndMenuBar();
                }
                ImGui::ProgressBar(context->axes[axis_i].input_fraction, { ImGui::GetContentRegionAvail().x, 0 }, fmt::format("{}", context->axes[axis_i].input).data());
                ImGui::SameLine();
                ImGui::Text("Raw Input");
                if (ImGui::Button(fmt::format(" {} ", ICON_FA_ANGLE_RIGHT).data())) {
                    context->axes_ex[axis_i].range_min = context->axes[axis_i].input;
                    update_axis_range = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Set the minimum range to the current raw input value.");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                ImGui::PushItemWidth(150);
                if (ImGui::InputInt("Min", &context->axes_ex[axis_i].range_min)) update_axis_range = true;
                ImGui::SameLine();
                if (ImGui::InputInt("Max", &context->axes_ex[axis_i].range_max)) update_axis_range = true;
                ImGui::PopItemWidth();
                ImGui::SameLine();
                if (ImGui::Button(fmt::format(" {} ", ICON_FA_ANGLE_LEFT).data())) {
                    context->axes_ex[axis_i].range_max = context->axes[axis_i].input;
                    update_axis_range = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Set the maximum range to the current raw input value.");
                    ImGui::EndTooltip();
                }
                if (ImGui::SliderInt("Output Limit##DZH", &context->axes_ex[axis_i].limit, 50, 100)) update_axis_range = true;
                if (!context->axes[axis_i].enabled) ImGui::PushStyleColor(ImGuiCol_FrameBg, { 72.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f });
                ImGui::ProgressBar(context->axes[axis_i].output_fraction, { ImGui::GetContentRegionAvail().x, 0 });
                if (!context->axes[axis_i].enabled) {
                    ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("This axis has been disabled.");
                        ImGui::EndTooltip();
                    }
                }
                if (update_axis_range) {
                    const auto err = context->handle->set_axis_range(axis_i, context->axes_ex[axis_i].range_min, context->axes_ex[axis_i].range_max, context->axes_ex[axis_i].limit);
                    if (err) spdlog::error(*err);
                    else spdlog::info("Updated axis #{} range: {}, {}, {}", axis_i, context->axes_ex[axis_i].range_min, context->axes_ex[axis_i].range_max, context->axes_ex[axis_i].limit);
                }
            }
            ImGui::EndChild();
            if (ImGui::BeginChild(fmt::format("##{}CurveWindow", label_default).data(), { 0, 294 }, true, ImGuiWindowFlags_MenuBar)) {
                if (ImGui::BeginMenuBar()) {
                    ImGui::Text(fmt::format("{} Curve", ICON_FA_BEZIER_CURVE).data());
                    ImGui::EndMenuBar();
                }
                const std::string selected_model_label = context->axes_ex[axis_i].model_edit_i >= 0 ? (context->models[context->axes_ex[axis_i].model_edit_i].label ? fmt::format("{} (#{})", *context->models[context->axes_ex[axis_i].model_edit_i].label, context->axes_ex[axis_i].model_edit_i) : fmt::format("Model #{}", context->axes_ex[axis_i].model_edit_i)) : "None selected.";
                if (ImGui::BeginCombo(fmt::format("##{}CurveOptions", label_default).data(), selected_model_label.data())) {
                    for (int model_i = 0; model_i < context->models.size(); model_i++) {
                        std::string this_label = context->models[model_i].label ? fmt::format("{} (#{})", *context->models[model_i].label, model_i) : fmt::format("Model #{}", model_i);
                        if (model_i == context->axes[axis_i].curve_i) this_label += " *";
                        if (ImGui::Selectable(this_label.data())) context->axes_ex[axis_i].model_edit_i = model_i;
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                ImGui::Text("Curve Model");
                if (context->axes_ex[axis_i].model_edit_i >= 0) {
                    ImGui::InputText("", context->models[context->axes_ex[axis_i].model_edit_i].label_buffer.data(), context->models[context->axes_ex[axis_i].model_edit_i].label_buffer.size());
                    ImGui::SameLine();
                    if (ImGui::Button("Set Label", { ImGui::GetContentRegionAvail().x, 0 })) {
                        const auto err = context->handle->set_bezier_label(context->axes_ex[axis_i].model_edit_i, context->models[context->axes_ex[axis_i].model_edit_i].label_buffer.data());
                        if (err) spdlog::error(*err);
                        else context->models[context->axes_ex[axis_i].model_edit_i].label = context->models[context->axes_ex[axis_i].model_edit_i].label_buffer.data();
                    }
                    {
                        std::vector<glm::dvec2> model;
                        for (auto &percent : context->models[context->axes_ex[axis_i].model_edit_i].points) model.push_back({
                            static_cast<double>(percent.x) / 100.0,
                            static_cast<double>(percent.y) / 100.0
                        });
                        float cif = static_cast<double>(context->axes[axis_i].input - context->axes[axis_i].min) / static_cast<double>(context->axes[axis_i].max - context->axes[axis_i].min);
                        if (cif > 1.0f) cif = 1.0f;
                        if (context->axes_ex[axis_i].model_edit_i == context->axes[axis_i].curve_i) cubic_bezier_plot(model, { 200, 200 }, context->axes[axis_i].output_fraction, std::nullopt, context->axes_ex[axis_i].limit / 100.f, cif);
                        else cubic_bezier_plot(model, { 200, 200 }, std::nullopt, std::nullopt, context->axes_ex[axis_i].limit / 100.f, cif);
                    }
                    ImGui::SameLine();
                    if (ImGui::BeginChild(fmt::format("##{}CurveWindowRightPanel", label_default).data(), { ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y }, false)) {
                        bool update_model = false;
                        ImGui::PushItemWidth(80);
                        for (int i = 0; i < context->models[context->axes_ex[axis_i].model_edit_i].points.size(); i++) {
                            if (i == 0 || i == context->models[context->axes_ex[axis_i].model_edit_i].points.size() - 1) continue;
                            switch (i) {
                                case 1: ImGui::TextDisabled("20%%"); break;
                                case 2: ImGui::TextDisabled("40%%"); break;
                                case 3: ImGui::TextDisabled("60%%"); break;
                                case 4: ImGui::TextDisabled("80%%"); break;
                                default: break;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button(fmt::format("{}##YM{}", ICON_FA_MINUS, i + 1).data()) && context->models[context->axes_ex[axis_i].model_edit_i].points[i].y > 0) {
                                context->models[context->axes_ex[axis_i].model_edit_i].points[i].y--;
                                update_model = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button(fmt::format("{}##YP{}", ICON_FA_PLUS, i + 1).data()) && context->models[context->axes_ex[axis_i].model_edit_i].points[i].y < 100) {
                                context->models[context->axes_ex[axis_i].model_edit_i].points[i].y++;
                                update_model = true;
                            }
                            ImGui::SameLine();
                            ImGui::SameLine();
                            if (ImGui::SliderInt(fmt::format("Y##{}", i + 1).data(), &context->models[context->axes_ex[axis_i].model_edit_i].points[i].y, 0, 100)) update_model = true;
                        }
                        ImGui::PopItemWidth();
                        if (update_model) {
                            std::array<glm::vec2, 6> model;
                            for (int i = 0; i < context->models[context->axes_ex[axis_i].model_edit_i].points.size(); i++) model[i] = {
                                static_cast<float>(context->models[context->axes_ex[axis_i].model_edit_i].points[i].x) / 100.f,
                                static_cast<float>(context->models[context->axes_ex[axis_i].model_edit_i].points[i].y) / 100.f
                            };
                            const auto err = context->handle->set_bezier_model(context->axes_ex[axis_i].model_edit_i, model);
                            if (err) spdlog::warn(*err);
                            else spdlog::info("Model updated.");
                        }
                        if (context->axes[axis_i].curve_i != context->axes_ex[axis_i].model_edit_i && ImGui::Button("Make Active", { ImGui::GetContentRegionAvail().x, 0 })) {
                            const auto err = context->handle->set_axis_bezier_index(axis_i, context->axes_ex[axis_i].model_edit_i);
                            if (err) spdlog::warn(*err);
                            else spdlog::info("Axis model index updated.");
                        }
                    }
                    ImGui::EndChild();
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }

    static void emit_content_device_panel() {
        enum class selection_type {
            axis,
            button,
            hat
        };
        if (device_contexts.size()) {
            animation_scan.playing = false;
            if (ImGui::BeginTabBar("##DeviceTabBar")) {
                for (const auto &context : device_contexts) {
                    std::lock_guard guard(context->mutex);
                    if (ImGui::BeginTabItem(fmt::format("{} {}##{}", ICON_FA_MICROCHIP, context->name, context->serial).data())) {
                        if (context->handle) {
                            ImGui::TextColored({ .2f, 1, .2f, 1 }, fmt::format("{} Connected.", ICON_FA_CHECK_DOUBLE).data());
                            ImGui::SameLine();
                            ImGui::TextDisabled(fmt::format("v{}.{}.{}", context->version_major, context->version_minor, context->version_revision).data());
                        } else ImGui::TextColored({ 1, 1, .2f, 1 }, fmt::format("{} Not connected.", ICON_FA_SPINNER).data());
                        if (context->initial_communication_complete) {
                            const auto top_y = ImGui::GetCursorScreenPos().y;
                            animation_comm.playing = false;
                            if (ImGui::BeginChild("##DeviceInteractionBox", { 200, 86 }, true, ImGuiWindowFlags_MenuBar)) {
                                if (ImGui::BeginMenuBar()) {
                                    ImGui::Text(fmt::format("{} Controls", ICON_FA_SATELLITE_DISH).data());
                                    ImGui::EndMenuBar();
                                }
                                if (ImGui::Button(fmt::format("{} Save to Memory", ICON_FA_FILE_IMPORT).data(), { ImGui::GetContentRegionAvail().x, 0 })) {
                                    const auto err = context->handle->commit();
                                    if (err) spdlog::error(*err);
                                    else spdlog::info("Settings saved.");
                                }
                                if (ImGui::Button(fmt::format("{} Clear Memory", ICON_FA_ERASER).data(), { ImGui::GetContentRegionAvail().x, 0 }));
                            }
                            ImGui::EndChild();
                            static int current_selection;
                            if (ImGui::BeginChild("##DeviceHardwareList", { 200, 0 }, true, ImGuiWindowFlags_MenuBar)) {
                                if (ImGui::BeginMenuBar()) {
                                    ImGui::Text(fmt::format("{} Inputs", ICON_FA_SITEMAP).data());
                                    ImGui::EndMenuBar();
                                }
                                if (const auto num_axes = context->axes.size(); num_axes) {
                                    for (int i = 0; i < num_axes; i++) {
                                        switch (i) {
                                            case 0: if (ImGui::Selectable("Throttle", current_selection == 0)) current_selection = 0; break;
                                            case 1: if (ImGui::Selectable("Brake", current_selection == 1)) current_selection = 1; break;
                                            case 2: if (ImGui::Selectable("Clutch", current_selection == 2)) current_selection = 2; break;
                                        }
                                    }
                                }
                            }
                            ImGui::EndChild();
                            ImGui::SameLine(0, ImGui::GetStyle().FramePadding.x);
                            ImGui::SetCursorScreenPos({ ImGui::GetCursorScreenPos().x, top_y });
                            emit_axis_profile_slice(context, current_selection);
                        } else {
                            if (!animation_comm.playing) animation_comm.time = 164.0 / animation_comm.frame_rate;
                            animation_comm.playing = true;
                            ImPenUtility pen;
                            pen.CalculateWindowBounds();
                            const auto image_pos = pen.GetCenteredPosition(GLMD_IM2(animation_comm.frames[animation_comm.frame_i]->size));
                            ImGui::SetCursorScreenPos(image_pos);
                            if (animation_comm.frames.size()) {
                                ImGui::Image(
                                    reinterpret_cast<ImTextureID>(animation_comm.frames[animation_comm.frame_i]->handle),
                                    GLMD_IM2(animation_comm.frames[animation_comm.frame_i]->size)
                                );
                            }
                        }
                        ImGui::EndTabItem();
                    }
                }
                ImGui::EndTabBar();
            }
        } else {
            animation_scan.play = true;
            ImPenUtility pen;
            pen.CalculateWindowBounds();
            const auto image_pos = pen.GetCenteredPosition(GLMD_IM2(animation_scan.frames[animation_scan.frame_i]->size));
            ImGui::SetCursorScreenPos(image_pos);
            if (animation_scan.frames.size()) {
                ImGui::Image(
                    reinterpret_cast<ImTextureID>(animation_scan.frames[animation_scan.frame_i]->handle),
                    GLMD_IM2(animation_scan.frames[animation_scan.frame_i]->size),
                    { 0, 0 },
                    { 1, 1 },
                    { 1, 1, 1, animation_scan.playing ? 1 : 0.8f }
                );
            }
        }
    }

    static void emit_content_panel() {
        emit_content_device_panel();
    }

    static void emit_primary_window_menu_bar() {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu(fmt::format("{} File", ICON_FA_FILE_CODE).data())) {
                // if (ImGui::Selectable(fmt::format("{} Theme", ICON_FA_PAINT_ROLLER).data()));
                if (ImGui::Selectable(fmt::format("{} Quit", ICON_FA_SKULL).data())) keep_running = false;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(fmt::format("{} Devices", ICON_FA_CUBES).data())) {
                if (devices.size() || device_contexts.size()) {
                    for (auto &device : devices) ImGui::TextDisabled(fmt::format("{} {} {} (#{})", ICON_FA_MICROCHIP, device->org, device->name, device->serial).data());
                    ImGui::Selectable(fmt::format("{} Clear System Calibrations", ICON_FA_ERASER).data());
                    if (ImGui::Selectable(fmt::format("{} Release All", ICON_FA_STOP).data())) {
                        devices.clear();
                        device_contexts.clear();
                    }
                } else ImGui::TextDisabled("No devices.");
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

void sc::visor::gui::initialize() {
    prepare_styling();
    load_animations();
    animation_scan.loop = true;
    animation_comm.loop = true;
}

void sc::visor::gui::shutdown() {
    devices_future = { };
    devices.clear();
    animation_scan.frames.clear();
    animation_comm.frames.clear();
}

void sc::visor::gui::emit(const glm::ivec2 &framebuffer_size, bool *const force_redraw) {
    bool animation_scan_updated = animation_scan.update();
    bool animation_comm_updated = animation_comm.update();
    if (force_redraw && (animation_scan_updated || animation_comm_updated)) *force_redraw = true;
    ImGui::SetNextWindowPos({ 0, 0 }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ static_cast<float>(framebuffer_size.x), static_cast<float>(framebuffer_size.y) }, ImGuiCond_Always);
    if (ImGui::Begin("##PrimaryWindow", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        emit_primary_window_menu_bar();
        emit_content_panel();
    }
    ImGui::End();
    poll_devices();
}