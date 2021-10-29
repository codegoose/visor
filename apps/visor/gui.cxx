#include "gui.h"
#include "application.h"
#include "animation_instance.h"
#include "device_context.h"
#include "legacy.h"

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
#include <magic_enum.hpp>

#include "../../libs/font/font_awesome_5.h"
#include "../../libs/font/font_awesome_5_brands.h"
#include "../../libs/imgui/imgui_utils.hpp"
#include "../../libs/defer.hpp"
#include "../../libs/resource/resource.h"
#include "../../libs/iracing/iracing.h"
#include "../../libs/api/api.h"

#include "bezier.h"
#include "im_glm_vec.hpp"

namespace sc::visor::gui {

    static animation_instance animation_scan, animation_comm;
    static std::optional<std::chrono::high_resolution_clock::time_point> devices_last_scan;
    static std::future<tl::expected<std::vector<std::shared_ptr<sc::firmware::mk4::device_handle>>, std::string>> devices_future;
    static std::vector<std::shared_ptr<firmware::mk4::device_handle>> devices;
    static std::vector<std::shared_ptr<device_context>> device_contexts;

    static bool legacy_is_default = true;
    static bool enable_legacy_support = false;

    static std::array<char, 92> account_email_input_buffer = { 0 };
    static std::array<char, 92> account_password_input_buffer = { 0 };
    static std::array<char, 92> account_name_input_buffer = { 0 };
    static std::optional<api::response> account_creation_response;
    static std::optional<std::string> account_creation_error;
    static std::optional<api::response> account_login_response;
    static std::optional<std::string> account_login_error;

    static void prepare_styling_colors(ImGuiStyle &style) {
        style.Colors[ImGuiCol_Tab] = { 50.f / 255.f, 50.f / 255.f, 50.f / 255.f, 1.f };
        style.Colors[ImGuiCol_TabActive] = { 70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.f };
        style.Colors[ImGuiCol_TabHovered] = { 90.f / 255.f, 90.f / 255.f, 90.f / 255.f, 1.f };
        style.Colors[ImGuiCol_WindowBg] = { 0x21 / 255.f, 0x25 / 255.f, 0x29 / 255.f, 1.f };
        style.Colors[ImGuiCol_ChildBg] = { 50.f / 255.f, 50.f / 255.f, 50.f / 255.f, 1.f };
        style.Colors[ImGuiCol_FrameBgActive] = { 70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.f };
        style.Colors[ImGuiCol_FrameBgHovered] = { 90.f / 255.f, 90.f / 255.f, 90.f / 255.f, 1.f };
        style.Colors[ImGuiCol_FrameBg] = { 42.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f };
        style.Colors[ImGuiCol_ModalWindowDimBg] = { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .88f };
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

    static void emit_axis_profile_slice(const std::shared_ptr<device_context> &context, int axis_i) {
        const auto label_default = axis_i == 0 ? "Throttle" : (axis_i == 1 ? "Brake" : "Clutch");
        if (ImGui::BeginChild(fmt::format("##{}Window", label_default).data(), { 0, 0 }, true, ImGuiWindowFlags_MenuBar)) {
            if (ImGui::BeginMenuBar()) {
                ImGui::Text(fmt::format("{} {} Configurations", ICON_FA_COGS, label_default).data());
                ImGui::EndMenuBar();
            }
            if (ImGui::Button(context->axes[axis_i].enabled ? fmt::format("{} Disable", ICON_FA_STOP).data() : fmt::format("{} Enable", ICON_FA_PLAY).data(), { ImGui::GetContentRegionAvail().x, 0 })) context->handle->set_axis_enabled(axis_i, !context->axes[axis_i].enabled);
            if (ImGui::BeginChild("##{}InputRangeWindow", { 0, 164 }, true, ImGuiWindowFlags_MenuBar)) {
                bool update_axis_range = false;
                if (ImGui::BeginMenuBar()) {
                    ImGui::Text(fmt::format("{} Range", ICON_FA_RULER).data());
                    ImGui::EndMenuBar();
                }
                ImGui::ProgressBar(context->axes[axis_i].input_fraction, { ImGui::GetContentRegionAvail().x, 0 }, fmt::format("{}", context->axes[axis_i].input).data());
                ImGui::SameLine();
                ImGui::Text("Raw Input");
                if (ImGui::Button(fmt::format(" {} Set Min ", ICON_FA_ARROW_TO_LEFT).data(), { 100, 0 })) {
                    context->axes_ex[axis_i].range_min = context->axes[axis_i].input;
                    update_axis_range = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Set the minimum range to the current raw input value.");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                ImGui::PushItemWidth(100);
                if (ImGui::InputInt("Min", &context->axes_ex[axis_i].range_min)) update_axis_range = true;
                ImGui::SameLine();
                if (ImGui::InputInt("Max", &context->axes_ex[axis_i].range_max)) update_axis_range = true;
                ImGui::PopItemWidth();
                ImGui::SameLine();
                if (ImGui::Button(fmt::format(" {} Set Max ", ICON_FA_ARROW_TO_RIGHT).data(), { 100, 0 })) {
                    context->axes_ex[axis_i].range_max = context->axes[axis_i].input;
                    update_axis_range = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Set the maximum range to the current raw input value.");
                    ImGui::EndTooltip();
                }
                if (ImGui::SliderInt("Deadzone", &context->axes_ex[axis_i].deadzone, 0, 30, "%d%%")) update_axis_range = true;
                if (ImGui::SliderInt("Output Limit##DZH", &context->axes_ex[axis_i].limit, 50, 100, "%d%%")) update_axis_range = true;
                if (!context->axes[axis_i].enabled) ImGui::PushStyleColor(ImGuiCol_FrameBg, { 72.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f });
                const auto old_y = ImGui::GetCursorPos().y;
                ImGui::SetCursorPos({ ImGui::GetCursorPos().x, ImGui::GetCursorPos().y + 2 });
                ImGui::Text(fmt::format("{}", ICON_FA_SIGNAL_SLASH).data());
                ImGui::SameLine();
                ImGui::SetCursorPos({ ImGui::GetCursorPos().x, old_y });
                const int deadzone_padding = (context->axes_ex[axis_i].deadzone / 100.f) * static_cast<float>(context->axes[axis_i].max - context->axes[axis_i].min);
                const float within_deadzone_fraction = context->axes[axis_i].input >= context->axes[axis_i].min ? (context->axes[axis_i].input < context->axes[axis_i].min + deadzone_padding ? (static_cast<float>(context->axes[axis_i].input - context->axes[axis_i].min) / static_cast<float>((context->axes[axis_i].min + deadzone_padding) - context->axes[axis_i].min)) : 1.f) : 0.f;
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 150.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f });
                if (context->axes_ex[axis_i].deadzone > 0) ImGui::ProgressBar(within_deadzone_fraction, { 80, 0 });
                else ImGui::ProgressBar(0.f, { 80, 0 }, "--");
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::Text(fmt::format("{}", ICON_FA_SIGNAL).data());
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 72.f / 255.f, 150.f / 255.f, 42.f / 255.f, 1.f });
                ImGui::ProgressBar(context->axes[axis_i].output_fraction, { ImGui::GetContentRegionAvail().x, 0 });
                ImGui::PopStyleColor();
                if (!context->axes[axis_i].enabled) {
                    ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("This axis has been disabled.");
                        ImGui::EndTooltip();
                    }
                }
                if (update_axis_range) {
                    const auto err = context->handle->set_axis_range(axis_i, context->axes_ex[axis_i].range_min, context->axes_ex[axis_i].range_max, context->axes_ex[axis_i].deadzone, context->axes_ex[axis_i].limit);
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
                        const int deadzone_padding = (context->axes_ex[axis_i].deadzone / 100.f) * static_cast<float>(context->axes[axis_i].max - context->axes[axis_i].min);
                        auto cif = glm::max(0.0, static_cast<double>(context->axes[axis_i].input - (context->axes[axis_i].min + deadzone_padding)) / static_cast<double>(context->axes[axis_i].max - (context->axes[axis_i].min + deadzone_padding)));
                        if (cif > 1.0) cif = 1.0;
                        if (context->axes_ex[axis_i].model_edit_i == context->axes[axis_i].curve_i) bezier::ui::plot_cubic(model, { 200, 200 }, context->axes[axis_i].output_fraction, std::nullopt, context->axes_ex[axis_i].limit / 100.f, cif);
                        else bezier::ui::plot_cubic(model, { 200, 200 }, std::nullopt, std::nullopt, context->axes_ex[axis_i].limit / 100.f, cif);
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
        if (ImGui::BeginTabBar("##AppModeBar")) {
            if (ImGui::BeginTabItem(fmt::format("{} Account", ICON_FA_USER).data())) {
                ImPenUtility pen;
                pen.CalculateWindowBounds();
                ImGui::SetCursorPos(pen.GetCenteredPosition({ 300, 400 }));
                if (ImGui::BeginChild("AccountEnablementChild", { 300, 400 }, true, ImGuiWindowFlags_NoScrollbar)) {
                    if (ImGui::BeginTabBar("##AccountEnablementTabBar")) {
                        auto flags = (account_creation_response || account_login_response) ? ImGuiInputTextFlags_ReadOnly : 0;
                        if (ImGui::BeginTabItem("Login")) {
                            if (account_login_response) {
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
                                if (account_login_response->valid()) {
                                    if (const auto status = account_login_response->wait_for(std::chrono::seconds(0)); status == std::future_status::ready) {
                                        const auto response = account_login_response->get();
                                        if (response.has_value()) {
                                            spdlog::debug(response->dump());
                                            
                                        } else {
                                            spdlog::error("Unable to login: {}", response.error());
                                            account_creation_error = response.error();
                                        }
                                        account_login_response.reset();
                                    }
                                }
                            } else {
                                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                                ImGui::InputTextWithHint("##AccountEnablementEmailInput", "email", account_email_input_buffer.data(), account_email_input_buffer.size(), flags);
                                ImGui::InputTextWithHint("##AccountEnablementPasswordInput", "password", account_password_input_buffer.data(), account_password_input_buffer.size(), flags);
                                ImGui::PopItemWidth();
                                if (ImGui::Button("Login", { ImGui::GetContentRegionAvail().x, 0 })) {
                                    spdlog::debug("Starting login attempt now.");
                                    account_login_response = api::customer::get_session_token(
                                        account_email_input_buffer.data(),
                                        account_password_input_buffer.data()
                                    );
                                    animation_scan.frame_i = 0;
                                }
                            }
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Create New")) {
                            if (account_creation_response) {
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
                                if (account_creation_response->valid()) {
                                    if (const auto status = account_creation_response->wait_for(std::chrono::seconds(0)); status == std::future_status::ready) {
                                        const auto response = account_creation_response->get();
                                        if (response.has_value()) {
                                            spdlog::debug(response->dump());
                                            
                                        } else {
                                            spdlog::error("Unable to create account: {}", response.error());
                                            account_creation_error = response.error();
                                        }
                                        account_creation_response.reset();
                                    }
                                }
                            } else {
                                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                                ImGui::InputTextWithHint("##AccountEnablementNameInput", "name", account_name_input_buffer.data(), account_name_input_buffer.size(), flags);
                                ImGui::InputTextWithHint("##AccountEnablementEmailInput", "e-mail", account_email_input_buffer.data(), account_email_input_buffer.size(), flags);
                                ImGui::InputTextWithHint("##AccountEnablementPasswordInput", "password", account_password_input_buffer.data(), account_password_input_buffer.size(), flags);
                                ImGui::PopItemWidth();
                                if (ImGui::Button("Create Account", { ImGui::GetContentRegionAvail().x, 0 })) {
                                    spdlog::debug("Starting account creation attempt now.");
                                    account_creation_response = api::customer::create_new(
                                        account_email_input_buffer.data(),
                                        account_name_input_buffer.data(),
                                        account_password_input_buffer.data()
                                    );
                                    animation_scan.frame_i = 0;
                                }
                            }
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(fmt::format("{} Hardware", ICON_FA_TOOLS).data())) {
                enum class selection_type {
                    axis,
                    button,
                    hat
                };
                if (device_contexts.size() || enable_legacy_support) {
                    animation_scan.playing = false;
                    if (ImGui::BeginTabBar("##DeviceTabBar")) {
                        for (const auto &context : device_contexts) {
                            std::lock_guard guard(context->mutex);
                            if (ImGui::BeginTabItem(fmt::format("{} {}##{}", ICON_FA_MICROCHIP, context->name, context->serial).data())) {
                                if (context->handle) {
                                    ImGui::TextColored({ .2f, 1, .2f, 1 }, fmt::format("{} Connected", ICON_FA_CHECK_DOUBLE).data());
                                    ImGui::SameLine();
                                    ImGui::TextDisabled(fmt::format("v{}.{}.{}", context->version_major, context->version_minor, context->version_revision).data());
                                } else ImGui::TextColored({ 1, 1, .2f, 1 }, fmt::format("{} Disconnected", ICON_FA_SPINNER).data());
                                if (context->initial_communication_complete) {
                                    const auto top_y = ImGui::GetCursorScreenPos().y;
                                    animation_comm.playing = false;
                                    if (ImGui::BeginChild("##DeviceInteractionBox", { 200, 86 }, true, ImGuiWindowFlags_MenuBar)) {
                                        if (ImGui::BeginMenuBar()) {
                                            ImGui::Text(fmt::format("{} Controls", ICON_FA_SATELLITE_DISH).data());
                                            ImGui::EndMenuBar();
                                        }
                                        if (ImGui::Button(fmt::format("{} Save to Chip", ICON_FA_FILE_IMPORT).data(), { ImGui::GetContentRegionAvail().x, 0 })) {
                                            const auto err = context->handle->commit();
                                            if (err) spdlog::error(*err);
                                            else spdlog::info("Settings saved.");
                                        }
                                        if (ImGui::Button(fmt::format("{} Clear Chip", ICON_FA_ERASER).data(), { ImGui::GetContentRegionAvail().x, 0 }));
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
                                                if (current_selection != i) {
                                                    ImGui::PushStyleColor(ImGuiCol_Button, { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .2f });
                                                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .3f });
                                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .4f });
                                                } else {
                                                    ImGui::PushStyleColor(ImGuiCol_Button, { 24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f });
                                                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f });
                                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f });
                                                }
                                                const auto label = (i == 0 ? "Throttle" : (i == 1 ? "Brake" : (i == 2 ? "Clutch" : "?")));
                                                if (ImGui::Button(label, { ImGui::GetContentRegionAvail().x, 40 })) current_selection = i;
                                                ImGui::PopStyleColor(3);
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
                        if (enable_legacy_support && ImGui::BeginTabItem(fmt::format("{} Virtual Pedals", ICON_FA_GHOST).data())) {
                            ImGui::TextColored({ .2f, 1, .2f, 1 }, fmt::format("{} Online", ICON_FA_CHECK_DOUBLE).data());
                            const auto top_y = ImGui::GetCursorScreenPos().y;
                            if (ImGui::BeginChild("##DeviceInteractionBox", { 200, 86 }, true, ImGuiWindowFlags_MenuBar)) {
                                if (ImGui::BeginMenuBar()) {
                                    ImGui::Text(fmt::format("{} Controls", ICON_FA_SATELLITE_DISH).data());
                                    ImGui::EndMenuBar();
                                }
                                if (ImGui::Button(fmt::format("{} Save Settings", ICON_FA_FILE_IMPORT).data(), { ImGui::GetContentRegionAvail().x, 0 })) {
                                    if (const auto err = legacy::save_settings(); err) {
                                        spdlog::error("Unable to save settings: {}", *err);
                                    } else spdlog::info("Settings saved.");
                                }
                                if (ImGui::Button(fmt::format("{} Clear Settings", ICON_FA_ERASER).data(), { ImGui::GetContentRegionAvail().x, 0 }));
                            }
                            ImGui::EndChild();
                            static int current_selection;
                            if (ImGui::BeginChild("##DeviceHardwareList", { 200, 0 }, true, ImGuiWindowFlags_MenuBar)) {
                                if (ImGui::BeginMenuBar()) {
                                    ImGui::Text(fmt::format("{} Inputs", ICON_FA_SITEMAP).data());
                                    ImGui::EndMenuBar();
                                }
                                for (int i = 0; i < legacy::axes.size(); i++) {
                                    // if (ImGui::Selectable(fmt::format("Axis #{}", i + 1).data(), current_selection == i, 0, { ImGui::GetContentRegionAvail().x, 40 })) current_selection = i;
                                    if (current_selection != i) {
                                        ImGui::PushStyleColor(ImGuiCol_Button, { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .2f });
                                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .3f });
                                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .4f });
                                    } else {
                                        ImGui::PushStyleColor(ImGuiCol_Button, { 24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f });
                                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f });
                                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f });
                                    }
                                    if (ImGui::Button(fmt::format("Axis #{}", i + 1).data(), { ImGui::GetContentRegionAvail().x, 40 })) current_selection = i;
                                    ImGui::PopStyleColor(3);
                                    ImGui::ProgressBar(legacy::axes[i].output, { ImGui::GetContentRegionAvail().x, 8 }, "");
                                }
                            }
                            ImGui::EndChild();
                            ImGui::SameLine(0, ImGui::GetStyle().FramePadding.x);
                            ImGui::SetCursorScreenPos({ ImGui::GetCursorScreenPos().x, top_y });
                            // !!!
                            if (ImGui::BeginChild(fmt::format("##VirtualAxis#{}_Window", current_selection).data(), { 0, 0 }, true, ImGuiWindowFlags_MenuBar)) {
                                if (ImGui::BeginMenuBar()) {
                                    ImGui::Text(fmt::format("{} Axis #{} Configurations", ICON_FA_COGS, current_selection + 1).data());
                                    ImGui::EndMenuBar();
                                }
                                if (ImGui::BeginChild("##{}InputRangeWindow", { 0, 164 }, true, ImGuiWindowFlags_MenuBar)) {
                                    bool update_axis_range = false;
                                    if (ImGui::BeginMenuBar()) {
                                        ImGui::Text(fmt::format("{} Range", ICON_FA_RULER).data());
                                        ImGui::EndMenuBar();
                                    }
                                    ImGui::ProgressBar(legacy::axes[current_selection].input_raw, { ImGui::GetContentRegionAvail().x, 0 }, fmt::format("{}", legacy::axes[current_selection].input_steps).data());
                                    ImGui::SameLine();
                                    ImGui::Text("Raw Input");
                                    if (ImGui::Button(fmt::format(" {} Set Min ", ICON_FA_ARROW_TO_LEFT).data(), { 100, 0 })) {
                                        legacy::axes[current_selection].output_steps_min = legacy::axes[current_selection].input_steps;
                                        update_axis_range = true;
                                    }
                                    if (ImGui::IsItemHovered()) {
                                        ImGui::BeginTooltip();
                                        ImGui::Text("Set the minimum range to the current raw input value.");
                                        ImGui::EndTooltip();
                                    }
                                    ImGui::SameLine();
                                    ImGui::PushItemWidth(100);
                                    if (ImGui::InputInt("Min", &legacy::axes[current_selection].output_steps_min)) update_axis_range = true;
                                    ImGui::SameLine();
                                    if (ImGui::InputInt("Max", &legacy::axes[current_selection].output_steps_max)) update_axis_range = true;
                                    ImGui::PopItemWidth();
                                    ImGui::SameLine();
                                    if (ImGui::Button(fmt::format(" {} Set Max ", ICON_FA_ARROW_TO_RIGHT).data(), { 100, 0 })) {
                                        legacy::axes[current_selection].output_steps_max = legacy::axes[current_selection].input_steps;
                                        update_axis_range = true;
                                    }
                                    if (ImGui::IsItemHovered()) {
                                        ImGui::BeginTooltip();
                                        ImGui::Text("Set the maximum range to the current raw input value.");
                                        ImGui::EndTooltip();
                                    }
                                    if (ImGui::SliderInt("Deadzone", &legacy::axes[current_selection].deadzone, 0, 30, "%d%%")) update_axis_range = true;
                                    if (ImGui::SliderInt("Output Limit##DZH", &legacy::axes[current_selection].output_limit, 50, 100, "%d%%")) update_axis_range = true;
                                    if (!legacy::axes[current_selection].present) ImGui::PushStyleColor(ImGuiCol_FrameBg, { 72.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f });
                                    const auto old_y = ImGui::GetCursorPos().y;
                                    ImGui::SetCursorPos({ ImGui::GetCursorPos().x, ImGui::GetCursorPos().y + 2 });
                                    ImGui::Text(fmt::format("{}", ICON_FA_SIGNAL_SLASH).data());
                                    ImGui::SameLine();
                                    ImGui::SetCursorPos({ ImGui::GetCursorPos().x, old_y });
                                    const int deadzone_padding = (legacy::axes[current_selection].deadzone / 100.f) * static_cast<float>(legacy::axes[current_selection].output_steps_max - legacy::axes[current_selection].output_steps_min);
                                    const float within_deadzone_fraction = legacy::axes[current_selection].input_steps >= legacy::axes[current_selection].output_steps_min ? (legacy::axes[current_selection].input_steps < legacy::axes[current_selection].output_steps_min + deadzone_padding ? (static_cast<float>(legacy::axes[current_selection].input_steps - legacy::axes[current_selection].output_steps_min) / static_cast<float>((legacy::axes[current_selection].output_steps_min + deadzone_padding) - legacy::axes[current_selection].output_steps_min)) : 1.f) : 0.f;
                                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 150.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f });
                                    if (legacy::axes[current_selection].deadzone > 0) ImGui::ProgressBar(within_deadzone_fraction, { 80, 0 });
                                    else ImGui::ProgressBar(0.f, { 80, 0 }, "--");
                                    ImGui::PopStyleColor();
                                    ImGui::SameLine();
                                    ImGui::Text(fmt::format("{}", ICON_FA_SIGNAL).data());
                                    ImGui::SameLine();
                                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 72.f / 255.f, 150.f / 255.f, 42.f / 255.f, 1.f });
                                    ImGui::ProgressBar(legacy::axes[current_selection].output, { ImGui::GetContentRegionAvail().x, 0 });
                                    ImGui::PopStyleColor();
                                    if (!legacy::axes[current_selection].present) {
                                        ImGui::PopStyleColor();
                                        if (ImGui::IsItemHovered()) {
                                            ImGui::BeginTooltip();
                                            ImGui::Text("This axis is not present.");
                                            ImGui::EndTooltip();
                                        }
                                    }
                                }
                                ImGui::EndChild();
                                if (ImGui::BeginChild(fmt::format("##Axis{}CurveWindow", current_selection).data(), { 0, 0 }, true, ImGuiWindowFlags_MenuBar)) {
                                    if (ImGui::BeginMenuBar()) {
                                        ImGui::Text(fmt::format("{} Curve", ICON_FA_BEZIER_CURVE).data());
                                        ImGui::EndMenuBar();
                                    }
                                    const std::string selected_model_label = legacy::axes[current_selection].model_edit_i >= 0 ? (legacy::models[legacy::axes[current_selection].model_edit_i].label ? fmt::format("{} (#{})", *legacy::models[legacy::axes[current_selection].model_edit_i].label, legacy::axes[current_selection].model_edit_i) : fmt::format("Model #{}", legacy::axes[current_selection].model_edit_i)) : "None selected.";
                                    if (ImGui::BeginCombo(fmt::format("##Axis{}CurveOptions", current_selection).data(), selected_model_label.data())) {
                                        for (int model_i = 0; model_i < legacy::models.size(); model_i++) {
                                            std::string this_label = legacy::models[model_i].label ? fmt::format("{} (#{})", *legacy::models[model_i].label, model_i) : fmt::format("Model #{}", model_i);
                                            if (model_i == legacy::axes[current_selection].curve_i) this_label += " *";
                                            if (ImGui::Selectable(this_label.data())) legacy::axes[current_selection].model_edit_i = model_i;
                                        }
                                        ImGui::EndCombo();
                                    }
                                    ImGui::SameLine();
                                    ImGui::Text("Curve Model");
                                    if (legacy::axes[current_selection].model_edit_i >= 0) {
                                        ImGui::InputText("", legacy::models[legacy::axes[current_selection].model_edit_i].label_buffer.data(), legacy::models[legacy::axes[current_selection].model_edit_i].label_buffer.size());
                                        ImGui::SameLine();
                                        if (ImGui::Button("Set Label", { ImGui::GetContentRegionAvail().x, 0 })) {
                                            legacy::models[legacy::axes[current_selection].model_edit_i].label = legacy::models[legacy::axes[current_selection].model_edit_i].label_buffer.data();
                                        }
                                        {
                                            std::vector<glm::dvec2> model;
                                            for (auto &percent : legacy::models[legacy::axes[current_selection].model_edit_i].points) model.push_back({
                                                static_cast<double>(percent.x) / 100.0,
                                                static_cast<double>(percent.y) / 100.0
                                            });
                                            const int deadzone_padding = (legacy::axes[current_selection].deadzone / 100.f) * static_cast<float>(legacy::axes[current_selection].output_steps_max - legacy::axes[current_selection].output_steps_min);
                                            auto cif = glm::max(0.0, static_cast<double>(legacy::axes[current_selection].input_steps - (legacy::axes[current_selection].output_steps_min + deadzone_padding)) / static_cast<double>(legacy::axes[current_selection].output_steps_max - (legacy::axes[current_selection].output_steps_min + deadzone_padding)));
                                            if (cif > 1.0) cif = 1.0;
                                            if (legacy::axes[current_selection].model_edit_i == legacy::axes[current_selection].curve_i) bezier::ui::plot_cubic(model, { 200, 200 }, legacy::axes[current_selection].output, std::nullopt, legacy::axes[current_selection].output_limit / 100.f, cif);
                                            else bezier::ui::plot_cubic(model, { 200, 200 }, std::nullopt, std::nullopt, legacy::axes[current_selection].output_limit / 100.f, cif);
                                        }
                                        ImGui::SameLine();
                                        if (ImGui::BeginChild(fmt::format("##Axis{}CurveWindowRightPanel", current_selection).data(), { ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y }, false)) {
                                            ImGui::PushItemWidth(80);
                                            for (int i = 0; i < legacy::models[legacy::axes[current_selection].model_edit_i].points.size(); i++) {
                                                if (i == 0 || i == legacy::models[legacy::axes[current_selection].model_edit_i].points.size() - 1) continue;
                                                switch (i) {
                                                    case 1: ImGui::TextDisabled("20%%"); break;
                                                    case 2: ImGui::TextDisabled("40%%"); break;
                                                    case 3: ImGui::TextDisabled("60%%"); break;
                                                    case 4: ImGui::TextDisabled("80%%"); break;
                                                    default: break;
                                                }
                                                ImGui::SameLine();
                                                if (ImGui::Button(fmt::format("{}##YM{}", ICON_FA_MINUS, i + 1).data()) && legacy::models[legacy::axes[current_selection].model_edit_i].points[i].y > 0) {
                                                    legacy::models[legacy::axes[current_selection].model_edit_i].points[i].y--;
                                                }
                                                ImGui::SameLine();
                                                if (ImGui::Button(fmt::format("{}##YP{}", ICON_FA_PLUS, i + 1).data()) && legacy::models[legacy::axes[current_selection].model_edit_i].points[i].y < 100) {
                                                    legacy::models[legacy::axes[current_selection].model_edit_i].points[i].y++;
                                                }
                                                ImGui::SameLine();
                                                if (ImGui::SliderInt(fmt::format("Y##{}", i + 1).data(), &legacy::models[legacy::axes[current_selection].model_edit_i].points[i].y, 0, 100));
                                            }
                                            ImGui::PopItemWidth();
                                            if (legacy::axes[current_selection].curve_i != legacy::axes[current_selection].model_edit_i && ImGui::Button("Make Active", { ImGui::GetContentRegionAvail().x, 0 })) {
                                                legacy::axes[current_selection].curve_i = legacy::axes[current_selection].model_edit_i;
                                            }
                                        }
                                        ImGui::EndChild();
                                    }
                                }
                                ImGui::EndChild();
                            }
                            ImGui::EndChild();
                            // !!!
                            ImGui::EndTabItem();
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
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(fmt::format("{} iRacing", ICON_FA_FLAG_CHECKERED).data())) {
                ImGui::Text(fmt::format("Status: {}", magic_enum::enum_name(iracing::get_status())).data());
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    static void emit_content_panel() {
        emit_content_device_panel();
    }

    static void try_toggle_legacy_support() {
        if (enable_legacy_support) {
            legacy::disable();
            enable_legacy_support = false;
        } else if (const auto err = legacy::enable(); err) {
            legacy_support_error = true;
            legacy_support_error_description = *err;
        } else enable_legacy_support = true;
    }

    static void emit_primary_window_menu_bar() {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu(fmt::format("{} File", ICON_FA_SAVE).data())) {
                if (ImGui::Selectable(fmt::format("{} Theme", ICON_FA_PAINT_ROLLER).data()));
                if (ImGui::Selectable(fmt::format("{} Quit", ICON_FA_SKULL).data())) keep_running = false;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(fmt::format("{} System", ICON_FA_CALCULATOR).data())) {
                if (!legacy_is_default) {
                    if (devices.size() || device_contexts.size()) {
                        for (auto &device : devices) ImGui::TextDisabled(fmt::format("{} {} {} (#{})", ICON_FA_MICROCHIP, device->org, device->name, device->serial).data());
                        if (ImGui::Selectable(fmt::format("{} Release Hardware", ICON_FA_STOP).data())) {
                            devices.clear();
                            device_contexts.clear();
                        }
                    } else ImGui::TextDisabled("No devices.");
                }
                ImGui::Selectable(fmt::format("{} Clear System Calibrations", ICON_FA_ERASER).data());
                if (!legacy_is_default && ImGui::Selectable(fmt::format("{} {} Legacy Support", ICON_FA_RECYCLE, enable_legacy_support ? "Disable" : "Enable").data())) {
                    try_toggle_legacy_support();
                }
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

    static void emit_legacy_hardware_enablement_error_popup(const glm::ivec2 &framebuffer_size) {
        ImGui::SetNextWindowPos({ 
            static_cast<float>((framebuffer_size.x / 2) - 200),
            static_cast<float>((framebuffer_size.y / 2) - 100)  
        }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ 400, 200 }, ImGuiCond_Always);
        // ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 12, 12 });
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, { 90.f / 255.f, 12.f / 255.f, 12.f / 255.f, 1.f });
        if (ImGui::BeginPopupModal(legacy_is_default ? "Hardware Enablement Error" : "Legacy Hardware Enablement Error", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::TextWrapped(fmt::format(
                "{} support was unable to be activated. There are additional drivers required for this functionality. Make sure they're installed.",
                legacy_is_default ? "Hardware" : "Legacy hardware"
            ).data());
            ImGui::NewLine();
            {
                const auto old_y = ImGui::GetCursorPos().y;
                ImGui::SetCursorPos({ ImGui::GetCursorPos().x, ImGui::GetCursorPos().y + 2 });
                ImGui::Text(fmt::format("{}", ICON_FA_EXCLAMATION_TRIANGLE).data());
                ImGui::SameLine();
                ImGui::SetCursorPos({ ImGui::GetCursorPos().x, old_y });
            }
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputText("##LegacySupportErrorDescription", legacy_support_error_description->data(), legacy_support_error_description->size(), ImGuiInputTextFlags_ReadOnly);
            ImGui::SetCursorPos({ ImGui::GetStyle().WindowPadding.x, ImGui::GetWindowSize().y - 30 - ImGui::GetStyle().WindowPadding.y });
            ImGui::PushStyleColor(ImGuiCol_Button, { 32.f / 255.f, 32.f / 255.f, 32.f / 255.f, 0.1f });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 32.f / 255.f, 32.f / 255.f, 32.f / 255.f, .5f });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 32.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f });
            if (ImGui::Button("Okay", { ImGui::GetContentRegionAvail().x, 30 })) ImGui::CloseCurrentPopup();
            ImGui::PopStyleColor(3);
            ImGui::EndPopup();
        }
        // ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
}

void sc::visor::gui::initialize() {
    prepare_styling();
    load_animations();
    animation_scan.loop = true;
    animation_comm.loop = true;
    if (legacy_is_default) try_toggle_legacy_support();
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
    emit_legacy_hardware_enablement_error_popup(framebuffer_size);
    if (legacy_support_error) {
        spdlog::error("Legacy support experienced an error.");
        ImGui::OpenPopup(legacy_is_default ? "Hardware Enablement Error" : "Legacy Hardware Enablement Error");
        legacy_support_error = false;
    }
    poll_devices();
}