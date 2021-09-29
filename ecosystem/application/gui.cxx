#include "gui.h"

#include <string_view>
#include <array>
#include <vector>
#include <optional>
#include <functional>
#include <future>
#include <imgui.h>
#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>
#include <pystring.h>

#include "application.h"

#include "../font/font_awesome_5.h"
#include "../font/font_awesome_5_brands.h"
#include "../imgui/imgui_utils.hpp"
#include "../defer.hpp"
#include "../resource/resource.h"
#include "../texture/texture.h"
#include "../file/file.h"
#include "../firmware/mk4.h"

#include <stb_image_write.h>

namespace sc::visor::gui {

    struct animation_instance {

        bool play = false;
        bool playing = false;
        bool loop = false;
        double time = 0;
        double frame_rate = 0;
        size_t frame_i = 0;
        std::vector<std::shared_ptr<sc::texture::gpu_handle>> frames;
        std::chrono::high_resolution_clock::time_point last_update = std::chrono::high_resolution_clock::now();

        bool update() {
            bool changed = false;
            const auto now = std::chrono::high_resolution_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);
            last_update = now;
            if (play && !playing) {
                time = 0;
                playing = true;
                play = false;
            }
            if (playing) {
                const double delta = static_cast<double>(elapsed.count()) / 1000.0;
                time += delta;
                if (const auto res = texture::frame_sequence::plot_frame_index(frame_rate, frames.size(), time, loop); res && frame_i != *res) {
                    frame_i = *res;
                    changed = true;
                }
                if (!loop && frame_i == frames.size() - 1) playing = false;
            }
            return changed;
        }
    };

    static animation_instance animation_scan, animation_comm;

    static std::optional<std::chrono::high_resolution_clock::time_point> devices_last_scan;
    static std::future<tl::expected<std::vector<std::shared_ptr<sc::firmware::mk4::device_handle>>, std::string>> devices_future;
    static std::vector<std::shared_ptr<firmware::mk4::device_handle>> devices;

    struct device_context {

        std::optional<std::chrono::high_resolution_clock::time_point> last_communication;
        std::shared_ptr<firmware::mk4::device_handle> handle;
        int version_major, version_minor, version_revision;
        std::string name, serial;
        std::optional<uint8_t> num_axes;
        std::vector<firmware::mk4::device_handle::axis_info> axes;
        bool initials_communications_completed = false;
    };

    static std::vector<std::shared_ptr<device_context>> device_contexts;

    static void prepare_styling_colors(ImGuiStyle &style) {
        style.Colors[ImGuiCol_Tab] = { 50.f / 255.f, 50.f / 255.f, 50.f / 255.f, 1.f };
        style.Colors[ImGuiCol_TabActive] = { 70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.f };
        style.Colors[ImGuiCol_TabHovered] = { 90.f / 255.f, 90.f / 255.f, 90.f / 255.f, 1.f };
        style.Colors[ImGuiCol_WindowBg] = { 30.f / 255.f, 30.f / 255.f, 30.f / 255.f, 1.f };
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

    static void scan_for_devices() {
        animation_scan.play = true;
        animation_scan.loop = true;
        devices_future = async(std::launch::async, []() {
            return sc::firmware::mk4::discover(devices);
        });
    }

    static void poll_devices() {
        if (devices_future.valid()) {
            if (devices_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                auto res = devices_future.get();
                DEFER({
                    devices_future = { };
                    animation_scan.loop = false;
                });
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
            if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - *devices_last_scan).count() >= 1) scan_for_devices();
        }
        for (auto &device : devices) {
            const auto contexts_i = std::find_if(device_contexts.begin(), device_contexts.end(), [&device](const std::shared_ptr<device_context> &context) {
                return context->serial == device->serial;
            });
            if (contexts_i != device_contexts.end()) {
                if (contexts_i->get()->handle.get() != device.get()) {
                    spdlog::debug("Applying new handle to existing device context. (Serial: {})", device->serial);
                    contexts_i->get()->handle = device;
                    contexts_i->get()->initials_communications_completed = false;
                }
                continue;
            }
            spdlog::debug("Creating new device context. (Serial: {})", device->serial);
            auto new_device_context = std::make_shared<device_context>();
            new_device_context->handle = device;
            new_device_context->name = device->name;
            new_device_context->serial = device->serial;
            device_contexts.push_back(new_device_context);
        }
        for (auto &context : device_contexts) {
            if (!context->handle) continue;
            const auto now = std::chrono::high_resolution_clock::now();
            if (!context->last_communication) context->last_communication = now;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *context->last_communication).count() >= 2) {
                context->last_communication = now;
                if (const auto res = context->handle->get_version(); res) {
                    context->version_major = std::get<0>(*res);
                    context->version_minor = std::get<1>(*res);
                    context->version_revision = std::get<2>(*res);
                } else spdlog::error("Device context error: {}", res.error());
                if (const auto res = context->handle->get_num_axes(); res) {
                    context->num_axes = *res;
                } else spdlog::error("Device context error: {}", res.error());
                context->axes.clear();
                for (int i = 0; i < *context->num_axes; i++) {
                    const auto res = context->handle->get_axis_state(i);
                    if (!res.has_value()) {
                        spdlog::error("Device context error: {}", res.error());
                        break;
                    }
                    context->axes.push_back(*res);
                }
                if (context->axes.size() == *context->num_axes) {
                    context->initials_communications_completed = true;
                    continue;
                }
                devices.erase(std::remove_if(devices.begin(), devices.end(), [&](const std::shared_ptr<firmware::mk4::device_handle> &device) {
                    return device.get() == context->handle.get();
                }), devices.end());
                context->initials_communications_completed = false;
                context->handle.reset();
            }
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

    static void emit_axis_profile_slice(const std::shared_ptr<device_context> &context, int axis_i) {
        const auto label_default = "Throttle";
        if (ImGui::BeginChild(fmt::format("##{}Window", label_default).data(), { 0, 0 }, true, ImGuiWindowFlags_MenuBar)) {
            if (ImGui::BeginMenuBar()) {
                ImGui::Text(fmt::format("{} {}", ICON_FA_CUBE, label_default).data());
                ImGui::EndMenuBar();
            }
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::ProgressBar(context->axes[axis_i].input_fraction);
            ImGui::ProgressBar(context->axes[axis_i].output_fraction);
            if (ImGui::Button(context->axes[axis_i].enabled ? "Disable this pedal." : "Enable this pedal.")) context->handle->set_axis_enabled(axis_i, !context->axes[axis_i].enabled);
            if (ImGui::BeginChild(fmt::format("##{}RangeWindow", label_default).data(), { 0, 120 }, true, ImGuiWindowFlags_MenuBar)) {
                if (ImGui::BeginMenuBar()) {
                    ImGui::Text(fmt::format("{} Range", ICON_FA_RULER).data());
                    ImGui::EndMenuBar();
                }
                static int dz_top = 0, dz_bottom = 0;
                ImGui::PushItemWidth(120);
                ImGui::Text(fmt::format("Minimum Range: {}", context->axes[axis_i].min).data());
                ImGui::Text(fmt::format("Maximum Range: {}", context->axes[axis_i].max).data());
                ImGui::Text(fmt::format("Raw Input: {}", context->axes[axis_i].input).data());
                ImGui::Text(fmt::format("Output: {}", context->axes[axis_i].output).data());
                ImGui::PopItemWidth();
            }
            ImGui::EndChild();
            if (ImGui::BeginChild(fmt::format("##{}DeadzoneWindow", label_default).data(), { 0, 86 }, true, ImGuiWindowFlags_MenuBar)) {
                if (ImGui::BeginMenuBar()) {
                    ImGui::Text(fmt::format("{} Deadzone", ICON_FA_ANCHOR).data());
                    ImGui::EndMenuBar();
                }
                static int dz_top = 0, dz_bottom = 0;
                ImGui::PushItemWidth(160);
                ImGui::SliderInt("Low Deadzone", &dz_bottom, 0, 15, "%d%%");
                ImGui::SliderInt("High Deadzone", &dz_top, 0, 15, "%d%%");
                ImGui::PopItemWidth();
            }
            ImGui::EndChild();
            /*
            if (ImGui::BeginChild(fmt::format("##{}CurveWindow", label_default).data(), { 340, 0 }, true, ImGuiWindowFlags_MenuBar)) {
                if (ImGui::BeginMenuBar()) {
                    ImGui::Text(fmt::format("{} Curve", ICON_FA_BEZIER_CURVE).data());
                    ImGui::EndMenuBar();
                }
                if (ImGui::BeginCombo(fmt::format("##{}CurveOptions", label_default).data(), "Linear")) {
                    ImGui::Selectable("Option 1");
                    ImGui::Selectable("Option 2");
                    ImGui::Selectable("Option 3");
                    ImGui::Selectable("Option 4");
                    ImGui::Selectable("Option 5");
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                ImGui::Text("Curve Type");
                {
                    std::vector<glm::dvec2> model;
                    for (auto &percent : axis.model) model.push_back({
                        static_cast<double>(percent.x) / 100.0,
                        static_cast<double>(percent.y) / 100.0
                    });
                    cubic_bezier_plot(model, { ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x });
                }
                ImGui::PushItemWidth(80);
                for (int i = 0; i < axis.model.size(); i++) {
                    if (i == 0 || i == axis.model.size() - 1) continue;
                    ImGui::TextDisabled(fmt::format("#{}", i + 1).data());
                    ImGui::SameLine();
                    if (ImGui::Button(fmt::format("{}##XM{}", ICON_FA_MINUS, i + 1).data()) && axis.model[i].x > 0) axis.model[i].x--;
                    ImGui::SameLine();
                    if (ImGui::Button(fmt::format("{}##XP{}", ICON_FA_PLUS, i + 1).data()) && axis.model[i].x < 100) axis.model[i].x++;
                    ImGui::SameLine();
                    if (ImGui::SliderInt(fmt::format("X##{}", i + 1).data(), &axis.model[i].x, 0, 100));
                    ImGui::SameLine();
                    if (ImGui::Button(fmt::format("{}##YM{}", ICON_FA_MINUS, i + 1).data()) && axis.model[i].y > 0) axis.model[i].y--;
                    ImGui::SameLine();
                    if (ImGui::Button(fmt::format("{}##YP{}", ICON_FA_PLUS, i + 1).data()) && axis.model[i].y < 100) axis.model[i].y++;
                    ImGui::SameLine();
                    ImGui::SameLine();
                    if (ImGui::SliderInt(fmt::format("Y##{}", i + 1).data(), &axis.model[i].y, 0, 100));
                }
                ImGui::PopItemWidth();
            }
            ImGui::EndChild();
            ImGui::SameLine();
            if (ImGui::BeginChild(fmt::format("##{}SettingsWindow", label_default).data(), { 0, 0 }, true, ImGuiWindowFlags_MenuBar)) {
                if (ImGui::BeginMenuBar()) {
                    ImGui::Text(fmt::format("{} Settings", ICON_FA_USER_COG).data());
                    ImGui::EndMenuBar();
                }
                if (ImGui::InputText(fmt::format("Label##{}TextInput", label_default).data(), axis.label_input_buf, sizeof(axis.label_input_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                    axis.label = axis.label_input_buf;
                    memset(axis.label_input_buf, 0, sizeof(axis.label_input_buf));
                }
            }
            ImGui::EndChild();
            */
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
            animation_scan.time = 0;
            animation_scan.playing = false;
            if (ImGui::BeginTabBar("##DeviceTabBar")) {
                for (const auto &context : device_contexts) {
                    if (ImGui::BeginTabItem(fmt::format("{} {}##{}", ICON_FA_MICROCHIP, context->name, context->serial).data())) {
                        if (context->handle) ImGui::TextColored({ .2f, 1, .2f, 1 }, fmt::format("{} Connected.", ICON_FA_CHECK_DOUBLE).data());
                        else ImGui::TextColored({ 1, 1, .2f, 1 }, fmt::format("{} Not connected.", ICON_FA_SPINNER).data());
                        if (context->num_axes) {
                            ImGui::SameLine();
                            ImGui::TextDisabled(fmt::format("{} axes.", context->axes.size()).data());
                        }
                        if (context->initials_communications_completed) {
                            animation_comm.playing = false;
                            if (ImGui::BeginTabBar("##DeviceSpecificsTabBar")) {
                                if (ImGui::BeginTabItem(fmt::format("{} Hardware", ICON_FA_COG).data())) {
                                    static std::optional<std::pair<selection_type, int>> current_selection;
                                    if (ImGui::BeginChild("##DeviceMiscInformation", { 0, 0 }, true)) {
                                        if (ImGui::BeginChild("##DeviceHardwareList", { 200, 0 }, true, ImGuiWindowFlags_MenuBar)) {
                                            if (ImGui::BeginMenuBar()) {
                                                ImGui::Text(fmt::format("{} Inputs", ICON_FA_SITEMAP).data());
                                                ImGui::EndMenuBar();
                                            }
                                            if (context->num_axes) {
                                                for (int i = 0; i < *context->num_axes; i++) {
                                                    switch (i) {
                                                        case 0: ImGui::Selectable("Throttle"); break;
                                                        case 1: ImGui::Selectable("Brake"); break;
                                                        case 2: ImGui::Selectable("Clutch"); break;
                                                    }
                                                }
                                            }
                                        }
                                        ImGui::EndChild();
                                        ImGui::SameLine();
                                        emit_axis_profile_slice(context, 0);
                                    }
                                    ImGui::EndChild();
                                    ImGui::EndTabItem();
                                }
                                if (ImGui::BeginTabItem(fmt::format("{} Profile", ICON_FA_SLIDERS_H).data())) {
                                    static std::optional<std::pair<selection_type, int>> current_selection;
                                    if (ImGui::BeginChild("##ProfileInformation", { 0, 0 }, true, ImGuiWindowFlags_MenuBar)) {
                                        if (ImGui::BeginMenuBar()) {
                                            ImGui::TextDisabled(fmt::format("{}", ICON_FA_FOLDER_OPEN).data());
                                            ImGui::SameLine();
                                            ImGui::SetNextItemWidth(180);
                                            if (ImGui::BeginCombo("##ProfileSelector", "Default")) {
                                                ImGui::Selectable("Default");
                                                ImGui::EndCombo();
                                            }
                                            ImGui::SameLine();
                                            ImGui::Button(fmt::format("{}##ButtonProfileDelete", ICON_FA_MINUS_SQUARE).data());
                                            ImGui::SameLine();
                                            ImGui::Button(fmt::format("{}##ButtonProfileAdd", ICON_FA_PLUS_SQUARE).data());
                                            ImGui::EndMenuBar();
                                        }
                                        if (ImGui::BeginChild("##ProfileInputList", { 200, 0 }, true, ImGuiWindowFlags_MenuBar)) {
                                            if (ImGui::BeginMenuBar()) {
                                                ImGui::Text(fmt::format("{} Inputs", ICON_FA_SITEMAP).data());
                                                ImGui::EndMenuBar();
                                            }
                                        }
                                        ImGui::EndChild();
                                        ImGui::SameLine();
                                        if (current_selection) {
                                            switch (current_selection->first) {
                                                case selection_type::axis:
                                                    // emit_axis_profile_slice(joy, current_selection->second);
                                                    break;
                                                case selection_type::button:
                                                    break;
                                                case selection_type::hat:
                                                    break;
                                            }
                                        }
                                    }
                                    ImGui::EndChild();
                                    ImGui::EndTabItem();
                                }
                                ImGui::EndTabBar();
                            }
                            ImGui::EndTabItem();
                        } else {
                            if (!animation_comm.playing) animation_comm.time = 164.0 / animation_comm.frame_rate;
                            animation_comm.playing = true;
                            animation_comm.loop = true;
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
                    }
                }
                ImGui::EndTabBar();
            }
        } else {
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
                if (devices.size()) {
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