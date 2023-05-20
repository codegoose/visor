#include "gui.h"  // Include GUI header file
#include "application.h"  // Include application header file
#include "animation_instance.h"  // Include animation instance header file
#include "device_context.h"  // Include device context header file
#include "legacy.h"  // Include legacy header file

#include <string_view>  // Include header for string views
#include <array>  // Include header for arrays
#include <vector>  // Include header for vectors
#include <queue>  // Include header for queues
#include <optional>  // Include header for optional values
#include <functional>  // Include header for function objects
#include <future>  // Include header for futures and promises
#include <mutex>  // Include header for mutexes
#include <memory>  // Include header for memory management
#include <imgui.h>  // Include ImGui library
#include <fmt/format.h>  // Include header for string formatting
#include <glm/common.hpp>  // Include header for common GLM functions
#include <glm/vec2.hpp>  // Include header for 2D vectors in GLM
#include <spdlog/spdlog.h>  // Include header for logging library
#include <pystring.h>  // Include header for string operations
#include <magic_enum.hpp>  // Include header for enum utilities
#include <nlohmann/json.hpp>  // Include header for JSON manipulation

#include "../../libs/file/file.h"  // Include custom file handling library
#include "../../libs/font/font_awesome_5.h"  // Include custom FontAwesome 5 font library
#include "../../libs/font/font_awesome_5_brands.h"  // Include custom FontAwesome 5 Brands font library
#include "../../libs/imgui/imgui_utils.hpp"  // Include custom ImGui utilities
#include "../../libs/defer.hpp"  // Include custom defer library
#include "../../libs/resource/resource.h"  // Include custom resource management library
#include "../../libs/iracing/iracing.h"  // Include custom iRacing integration library
#include "../../libs/api/api.h"  // Include custom API library

#include "bezier.h"  // Include custom Bezier library
#include "im_glm_vec.hpp"  // Include custom GLM vector utilities

#include <windows.h>  // Include Windows API header
#include <shellapi.h>  // Include Shell API header

#undef min  // Undefine min macro
#undef max  // Undefine max macro

namespace sc::visor::gui {
    // Define struct for popup state
    struct popup_state {
        popup info;  // Popup information
        bool launched = false;  // Flag indicating if the popup has been launched
        std::shared_ptr<bool> open = std::make_shared<bool>(true);  // Shared pointer to a boolean indicating if the popup is open
    };

    static std::vector<popup_state> popups;  // Static vector to store popups

    // Generate a unique identifier based on an optional label
    static std::string uuid(const std::optional<std::string>& label = std::nullopt) {
        static uint64_t next_uuid = 0;  // Static counter for UUID generation
        return fmt::format("{}##UUID_{}", label ? *label : "", next_uuid++);  // Format and return the UUID string
    }

    // Emit popups and display them on the screen
    void popups_emit(const glm::ivec2& framebuffer_size) {
        for (auto& e : popups) {
            // Set the position of the next window based on the framebuffer size and popup size
            ImGui::SetNextWindowPos({
                static_cast<float>((framebuffer_size.x / 2) - (e.info.size.x / 2)),
                static_cast<float>((framebuffer_size.y / 2) - (e.info.size.y / 2))
            }, ImGuiCond_Appearing);
            // Set the size of the next window based on the popup size
            ImGui::SetNextWindowSize({
                static_cast<float>(e.info.size.x),
                static_cast<float>(e.info.size.y)
            }, ImGuiCond_Appearing);
            if (ImGui::BeginPopupModal(e.info.label.data(), e.open.get())) {
                e.info.emit();  // Emit the popup content
                ImGui::EndPopup();  // End the popup window
            }
        }
        for (auto& e : popups) {
            if (e.launched) continue;
            spdlog::debug("Launching popup: {}", e.info.label);
            ImGui::OpenPopup(e.info.label.data());  // Open the popup window
            e.launched = true;
        }
    }

    // Clean up closed popups
    void popups_cleanup() {
        for (int i = 0; i < popups.size(); i++) {
            if (*popups[i].open) continue;
            spdlog::debug("Closing popup: {}", popups[i].info.label);
            popups.erase(popups.begin() + i);  // Remove the closed popup from the vector
            i--;
        }
    }
}

// Launch a popup and add it to the popups vector
void sc::visor::gui::popup::launch() {
    popups.push_back({
        {
            uuid(this->label),  // Generate a UUID for the popup label
            this->size,  // Set the popup size
            this->emit  // Set the function to emit the popup content
        }
    });
}namespace sc::visor::gui {

    static nlohmann::json cfg;  // Static variable to store JSON configuration

    // Static variables related to animations and devices
    static animation_instance animation_scan, animation_comm, animation_under_construction;
    static std::optional<std::chrono::high_resolution_clock::time_point> devices_last_scan;  // Last time devices were scanned
    static std::future<tl::expected<std::vector<std::shared_ptr<sc::firmware::mk4::device_handle>>, std::string>> devices_future;  // Future for asynchronous device scanning
    static std::vector<std::shared_ptr<firmware::mk4::device_handle>> devices;  // Vector to store device handles
    static std::vector<std::shared_ptr<device_context>> device_contexts;  // Vector to store device contexts

    static bool legacy_is_default = true;  // Flag indicating if legacy support is enabled by default
    static bool enable_legacy_support = false;  // Flag indicating if legacy support is enabled

    // Variables related to account management
    static bool should_verify_session_token = false;  // Flag indicating if session token should be verified
    static std::optional<std::string> account_session_token;  // Optional session token
    static std::optional<std::string> account_person_name;  // Optional account person name
    static std::optional<std::string> account_email;  // Optional account email
    static std::array<char, 92> account_email_input_buffer = { 0 };  // Input buffer for account email
    static std::array<char, 92> account_password_input_buffer = { 0 };  // Input buffer for account password
    static std::array<char, 92> account_name_input_buffer = { 0 };  // Input buffer for account name
    static std::array<char, 92> account_code_input_buffer = { 0 };  // Input buffer for account code
    static std::optional<api::response> account_creation_response;  // Optional account creation response
    static std::optional<std::string> account_creation_error;  // Optional account creation error
    static std::optional<api::response> account_login_response;  // Optional account login response
    static std::optional<std::string> account_login_error;  // Optional account login error
    static std::optional<api::response> account_confirmation_response;  // Optional account confirmation response
    static std::optional<std::string> account_confirmation_error;  // Optional account confirmation error
    static std::optional<api::response> account_pw_reset_response;  // Optional account password reset response
    static std::optional<std::string> account_pw_reset_error;  // Optional account password reset error
    static std::optional<api::response> account_pw_reset_confirm_response;  // Optional account password reset confirmation response
    static std::optional<std::string> account_pw_reset_confirm_error;  // Optional account password reset confirmation error
    static bool account_remember_me = false;  // Flag indicating if account should be remembered
    static bool account_pw_reset_awaits = false;  // Flag indicating if password reset is awaited
    static bool account_activation_awaits = false;  // Flag indicating if account activation is awaited

    // Load configuration from a file
    static std::optional<std::string> cfg_load() {
        const auto load_res = file::load("settings.json");  // Load file "settings.json"
        if (!load_res.has_value()) return load_res.error();  // Return error if file loading fails
        cfg = nlohmann::json::parse(*load_res);  // Parse loaded JSON content
        if (cfg.is_null()) cfg = nlohmann::json::object();  // Initialize as empty JSON object if null
        return std::nullopt;  // Return no error
    }

    // Save configuration to a file
    static std::optional<std::string> cfg_save() {
        auto doc_content = cfg.dump(4);  // Convert JSON to string with indentation of 4 spaces
        std::vector<std::byte> doc_data;  // Vector to store byte data
        doc_data.resize(doc_content.size());  // Resize vector to match string size
        memcpy(doc_data.data(), doc_content.data(), glm::min(doc_data.size(), doc_content.size()));  // Copy string data to byte vector
        if (const auto err = file::save("settings.json", doc_data); err) return *err;  // Save byte vector to file "settings.json"
        return std::nullopt;  // Return no error
    }

    // Prepare styling colors for ImGui
    static void prepare_styling_colors(ImGuiStyle &style) {
        style.Colors[ImGuiCol_Tab] = { 50.f / 255.f, 50.f / 255.f, 50.f / 255.f, 1.f };  // Set Tab color
        style.Colors[ImGuiCol_TabActive] = { 70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.f };  // Set active Tab color
        style.Colors[ImGuiCol_TabHovered] = { 90.f / 255.f, 90.f / 255.f, 90.f / 255.f, 1.f };  // Set hovered Tab color
        style.Colors[ImGuiCol_WindowBg] = { 0x21 / 255.f, 0x25 / 255.f, 0x29 / 255.f, 1.f };  // Set Window background color
        style.Colors[ImGuiCol_ChildBg] = { 50.f / 255.f, 50.f / 255.f, 50.f / 255.f, 1.f };  // Set Child background color
        style.Colors[ImGuiCol_FrameBgActive] = { 70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.f };  // Set active Frame background color
        style.Colors[ImGuiCol_FrameBgHovered] = { 90.f / 255.f, 90.f / 255.f, 90.f / 255.f, 1.f };  // Set hovered Frame background color
        style.Colors[ImGuiCol_FrameBg] = { 42.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f };  // Set Frame background color
        style.Colors[ImGuiCol_ModalWindowDimBg] = { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .88f };  // Set Modal Window dim background color
    }

    // Prepare styling parameters for ImGui
    static void prepare_styling_parameters(ImGuiStyle &style) {
        style.WindowBorderSize = 1;  // Set Window border size
        style.FrameBorderSize = 1;  // Set Frame border size
        style.FrameRounding = 3.f;  // Set Frame rounding
        style.ChildRounding = 3.f;  // Set Child rounding
        style.ScrollbarRounding = 3.f;  // Set Scrollbar rounding
        style.WindowRounding = 3.f;  // Set Window rounding
        style.GrabRounding = 3.f;  // Set Grab rounding
        style.TabRounding = 3.f;  // Set Tab rounding
        style.Colors[ImGuiCol_ChildBg] = { .09f, .09f, .09f, 1.f };  // Set Child background color
    }

    // Prepare styling for ImGui
    static std::optional<std::string> prepare_styling() {
        auto &style = ImGui::GetStyle();  // Get ImGui style
        prepare_styling_parameters(style);  // Prepare styling parameters
        prepare_styling_colors(style);  // Prepare styling colors
        return std::nullopt;  // Return no error
    }

    // Prepare animation by loading and processing a resource
    void prepare_animation(const std::string_view &resource_name, animation_instance &instance, const glm::ivec2 &size) {
        if (const auto content = sc::resource::get_resource("DATA", resource_name); content) {  // Get resource content
            std::vector<std::byte> buffer(content->second);  // Create buffer for content
            memcpy(buffer.data(), content->first, buffer.size());  // Copy content to buffer
            if (const auto sequence = sc::texture::load_lottie_from_memory(resource_name, buffer, size); sequence.has_value()) {  // Load and process Lottie sequence from buffer
                instance.frame_rate = sequence->frame_rate;  // Set animation frame rate
                int frame_i = 0;  // Initialize frame index
                for (const auto &frame : sequence->frames) {  // Iterate through frames
                    const auto description = pystring::lower(fmt::format("<rsc:{}:{}x{}#{}>", resource_name, sequence->frames.front().size.x, sequence->frames.front().size.y, frame_i++));  // Generate frame description
                    if (const auto texture = sc::texture::upload_to_gpu(frame, sequence->frames.front().size, description); texture.has_value()) {  // Upload frame texture to GPU
                        instance.frames.push_back(*texture);  // Add texture to animation instance
                    } else spdlog::error(texture.error());  // Log error if texture upload fails
                }
            }
        }
    }
}
void load_animations() {
    prepare_animation("LOTTIE_LOADING", animation_scan, { 400, 400 });  // Load and prepare animation for loading
    prepare_animation("LOTTIE_COMMUNICATING", animation_comm, { 200, 200 });  // Load and prepare animation for communication
    prepare_animation("LOTTIE_UNDER_CONSTRUCTION", animation_under_construction, { 400, 400 });  // Load and prepare animation for under construction
}

// Poll devices and handle device updates
static void poll_devices() {
    if (devices_future.valid()) {
        if (devices_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto res = devices_future.get();  // Get the result of the future
            DEFER(devices_future = { });  // Reset the future
            if (!res.has_value()) {
                spdlog::error(res.error());  // Log the error
                return;
            }
            for (auto& new_device : *res) devices.push_back(new_device);  // Add new devices to the device vector
            devices_last_scan = std::chrono::high_resolution_clock::now();  // Update the last scan time
            if (res->size()) spdlog::debug("Found {} devices.", res->size());  // Log the number of found devices
        }
    } else {
        if (!devices_last_scan) devices_last_scan = std::chrono::high_resolution_clock::now();  // Check if it's the first scan
        if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - *devices_last_scan).count() >= 1) {
            devices_future = async(std::launch::async, []() {
                return sc::firmware::mk4::discover(devices);  // Discover devices asynchronously
            });
        }
    }
    for (auto& device : devices) {
        const auto contexts_i = std::find_if(device_contexts.begin(), device_contexts.end(), [&device](const std::shared_ptr<device_context>& context) {
            return context->serial == device->serial;  // Find the device context with matching serial
        });
        if (contexts_i != device_contexts.end()) {
            if (contexts_i->get()->handle.get() != device.get()) {
                spdlog::debug("Applied new handle to device context: {}", device->serial);  // Log the applied handle
                contexts_i->get()->handle = device;  // Update the device handle in the context
                contexts_i->get()->initial_communication_complete = false;  // Reset initial communication status
            }
            continue;
        }
        spdlog::debug("Created new device context: {}", device->serial);  // Log the creation of a new device context
        auto new_device_context = std::make_shared<device_context>();  // Create a new device context
        new_device_context->handle = device;  // Set the device handle in the context
        new_device_context->name = device->name;  // Set the device name in the context
        new_device_context->serial = device->serial;  // Set the device serial in the context
        device_contexts.push_back(new_device_context);  // Add the new device context to the vector
    }
    for (auto& context : device_contexts) {
        if (!context->handle) continue;  // Skip if the device handle is not set
        if (context->update_future.valid()) {
            if (context->update_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                const auto err = context->update_future.get();  // Get the result of the future
                if (!err.has_value()) continue;
                spdlog::error("Device context error: {}", *err);  // Log the device context error
                devices.erase(std::remove_if(devices.begin(), devices.end(), [&](const std::shared_ptr<firmware::mk4::device_handle>& device) {
                    return device.get() == context->handle.get();
                }), devices.end());  // Erase the device handle from the vector
                context->initial_communication_complete = false;  // Reset initial communication status
                context->handle.reset();  // Reset the device handle
            } else continue;
        }
        context->update_future = async(std::launch::async, [context]() {
            return device_context::update(context);  // Update the device context asynchronously
        });
    }
}

// Emit axis profile slice GUI
static void emit_axis_profile_slice(const std::shared_ptr<device_context>& context, int axis_i) {
    const auto label_default = axis_i == 0 ? "Throttle" : (axis_i == 1 ? "Brake" : "Clutch");  // Determine the label based on the axis index
    if (ImGui::BeginChild(fmt::format("##{}Window", label_default).data(), { 0, 0 }, true, ImGuiWindowFlags_MenuBar)) {
        if (ImGui::BeginMenuBar()) {
            ImGui::Text(fmt::format("{} {} Configurations", ICON_FA_COGS, label_default).data());  // Display the axis configuration label
            ImGui::EndMenuBar();
        }
        if (ImGui::Button(context->axes[axis_i].enabled ? fmt::format("{} Disable", ICON_FA_STOP).data() : fmt::format("{} Enable", ICON_FA_PLAY).data(), { ImGui::GetContentRegionAvail().x, 0 })) context->handle->set_axis_enabled(axis_i, !context->axes[axis_i].enabled);  // Toggle axis enable/disable
        if (ImGui::BeginChild("##{}InputRangeWindow", { 0, 164 }, true, ImGuiWindowFlags_MenuBar)) {
            bool update_axis_range = false;
            if (ImGui::BeginMenuBar()) {
                ImGui::Text(fmt::format("{} Range", ICON_FA_RULER).data());  // Display the axis range label
                ImGui::EndMenuBar();
            }
            ImGui::ProgressBar(context->axes[axis_i].input_fraction, { ImGui::GetContentRegionAvail().x, 0 }, fmt::format("{}", context->axes[axis_i].input).data());  // Display the input progress bar
            ImGui::SameLine();
            ImGui::Text("Raw Input");
            if (ImGui::Button(fmt::format(" {} Set Min ", ICON_FA_ARROW_TO_LEFT).data(), { 100, 0 })) {
                context->axes_ex[axis_i].range_min = context->axes[axis_i].input;  // Set the minimum range to the current raw input value
                update_axis_range = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Set the minimum range to the current raw input value.");  // Tooltip for setting the minimum range
                ImGui::EndTooltip();
            }
            ImGui::SameLine();
            ImGui::PushItemWidth(100);
            if (ImGui::InputInt("Min", &context->axes_ex[axis_i].range_min)) update_axis_range = true;  // Input field for minimum range
            ImGui::SameLine();
            if (ImGui::InputInt("Max", &context->axes_ex[axis_i].range_max)) update_axis_range = true;  // Input field for maximum range
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button(fmt::format(" {} Set Max ", ICON_FA_ARROW_TO_RIGHT).data(), { 100, 0 })) {
                context->axes_ex[axis_i].range_max = context->axes[axis_i].input;  // Set the maximum range to the current raw input value
                update_axis_range = true;
            }
if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Set the maximum range to the current raw input value.");  // Display tooltip for setting maximum range
    ImGui::EndTooltip();
}
if (ImGui::SliderInt("Deadzone", &context->axes_ex[axis_i].deadzone, 0, 30, "%d%%")) update_axis_range = true;  // Slider for deadzone value
if (ImGui::SliderInt("Output Limit##DZH", &context->axes_ex[axis_i].limit, 50, 100, "%d%%")) update_axis_range = true;  // Slider for output limit value
if (!context->axes[axis_i].enabled) ImGui::PushStyleColor(ImGuiCol_FrameBg, { 72.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f });  // Modify frame background color for disabled axis
const auto old_y = ImGui::GetCursorPos().y;
ImGui::SetCursorPos({ ImGui::GetCursorPos().x, ImGui::GetCursorPos().y + 2 });
ImGui::Text(fmt::format("{}", ICON_FA_SIGNAL_SLASH).data());  // Display signal slash icon
ImGui::SameLine();
ImGui::SetCursorPos({ ImGui::GetCursorPos().x, old_y });
const int deadzone_padding = (context->axes_ex[axis_i].deadzone / 100.f) * static_cast<float>(context->axes[axis_i].max - context->axes[axis_i].min);  // Calculate deadzone padding
const float within_deadzone_fraction = context->axes[axis_i].input >= context->axes[axis_i].min ? (context->axes[axis_i].input < context->axes[axis_i].min + deadzone_padding ? (static_cast<float>(context->axes[axis_i].input - context->axes[axis_i].min) / static_cast<float>((context->axes[axis_i].min + deadzone_padding) - context->axes[axis_i].min)) : 1.f) : 0.f;  // Calculate fraction within deadzone
ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 150.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f });  // Modify plot histogram color
if (context->axes_ex[axis_i].deadzone > 0) ImGui::ProgressBar(within_deadzone_fraction, { 80, 0 });  // Display progress bar for deadzone
else ImGui::ProgressBar(0.f, { 80, 0 }, "--");  // Display placeholder progress bar
ImGui::PopStyleColor();
ImGui::SameLine();
ImGui::Text(fmt::format("{}", ICON_FA_SIGNAL).data());  // Display signal icon
ImGui::SameLine();
ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 72.f / 255.f, 150.f / 255.f, 42.f / 255.f, 1.f });  // Modify plot histogram color
ImGui::ProgressBar(context->axes[axis_i].output_fraction, { ImGui::GetContentRegionAvail().x, 0 });  // Display progress bar for output fraction
ImGui::PopStyleColor();
if (!context->axes[axis_i].enabled) {
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("This axis has been disabled.");  // Display tooltip for disabled axis
        ImGui::EndTooltip();
    }
}
if (update_axis_range) {
    const auto err = context->handle->set_axis_range(axis_i, context->axes_ex[axis_i].range_min, context->axes_ex[axis_i].range_max, context->axes_ex[axis_i].deadzone, context->axes_ex[axis_i].limit);  // Update axis range
    if (err) spdlog::error(*err);  // Log error if range update failed
    else spdlog::info("Updated axis #{} range: {}, {}, {}", axis_i, context->axes_ex[axis_i].range_min, context->axes_ex[axis_i].range_max, context->axes_ex[axis_i].limit);  // Log successful range update
}
ImGui::EndChild();
if (ImGui::BeginChild(fmt::format("##{}CurveWindow", label_default).data(), { 0, 294 }, true, ImGuiWindowFlags_MenuBar)) {
    if (ImGui::BeginMenuBar()) {
        ImGui::Text(fmt::format("{} Curve", ICON_FA_BEZIER_CURVE).data());  // Display curve label in the menu bar
        ImGui::EndMenuBar();
    }
    const std::string selected_model_label = context->axes_ex[axis_i].model_edit_i >= 0 ? (context->models[context->axes_ex[axis_i].model_edit_i].label ? fmt::format("{} (#{})", *context->models[context->axes_ex[axis_i].model_edit_i].label, context->axes_ex[axis_i].model_edit_i) : fmt::format("Model #{}", context->axes_ex[axis_i].model_edit_i)) : "None selected.";  // Generate label for the selected curve model
    if (ImGui::BeginCombo(fmt::format("##{}CurveOptions", label_default).data(), selected_model_label.data())) {
        for (int model_i = 0; model_i < context->models.size(); model_i++) {
            std::string this_label = context->models[model_i].label ? fmt::format("{} (#{})", *context->models[model_i].label, model_i) : fmt::format("Model #{}", model_i);  // Generate label for each curve model
            if (model_i == context->axes[axis_i].curve_i) this_label += " *";  // Append "*" to label if it is the current curve model
            if (ImGui::Selectable(this_label.data())) context->axes_ex[axis_i].model_edit_i = model_i;  // Handle selection of curve model
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::Text("Curve Model");
    if (context->axes_ex[axis_i].model_edit_i >= 0) {
        ImGui::InputText("", context->models[context->axes_ex[axis_i].model_edit_i].label_buffer.data(), context->models[context->axes_ex[axis_i].model_edit_i].label_buffer.size());  // Input field for curve model label
        ImGui::SameLine();
        if (ImGui::Button("Set Label", { ImGui::GetContentRegionAvail().x, 0 })) {
            const auto err = context->handle->set_bezier_label(context->axes_ex[axis_i].model_edit_i, context->models[context->axes_ex[axis_i].model_edit_i].label_buffer.data());  // Set curve model label
            if (err) spdlog::error(*err);  // Log error if label setting failed
            else context->models[context->axes_ex[axis_i].model_edit_i].label = context->models[context->axes_ex[axis_i].model_edit_i].label_buffer.data();  // Update label value
        }
        {
            std::vector<glm::dvec2> model;
            for (auto &percent : context->models[context->axes_ex[axis_i].model_edit_i].points) model.push_back({
                static_cast<double>(percent.x) / 100.0,
                static_cast<double>(percent.y) / 100.0
            });  // Generate vector of points for the curve model
            const int deadzone_padding = (context->axes_ex[axis_i].deadzone / 100.f) * static_cast<float>(context->axes[axis_i].max - context->axes[axis_i].min);  // Calculate deadzone padding
            auto cif = glm::max(0.0, static_cast<double>(context->axes[axis_i].input - (context->axes[axis_i].min + deadzone_padding)) / static_cast<double>(context->axes[axis_i].max - (context->axes[axis_i].min + deadzone_padding)));  // Calculate current input fraction within the curve model
            if (cif > 1.0) cif = 1.0;  // Limit input fraction to 1.0
            if (context->axes_ex[axis_i].model_edit_i == context->axes[axis_i].curve_i) bezier::ui::plot_cubic(model, { 200, 200 }, context->axes[axis_i].output_fraction, std::nullopt, context->axes_ex[axis_i].limit / 100.f, cif);  // Display cubic curve plot
            else bezier::ui::plot_cubic(model, { 200, 200 }, std::nullopt, std::nullopt, context->axes_ex[axis_i].limit / 100.f, cif);  // Display cubic curve plot without output fraction
        }
        ImGui::SameLine();
        if (ImGui::BeginChild(fmt::format("##{}CurveWindowRightPanel", label_default).data(), { ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y }, false)) {
            bool update_model = false;
            ImGui::PushItemWidth(80);
            for (int i = 0; i < context->models[context->axes_ex[axis_i].model_edit_i].points.size(); i++) {
                if (i == 0 || i == context->models[context->axes_ex[axis_i].model_edit_i].points.size() - 1) continue;  // Skip the first and last points
                switch (i) {
                    case 1: ImGui::TextDisabled("20%%"); break;  // Display label for the second point
                    case 2: ImGui::TextDisabled("40%%"); break;  // Display label for the third point
                    case 3: ImGui::TextDisabled("60%%"); break;  // Display label for the fourth point
                    case 4: ImGui::TextDisabled("80%%"); break;  // Display label for the fifth point
                    default: break;
                }
                ImGui::SameLine();
                if (ImGui::Button(fmt::format("{}##YM{}", ICON_FA_MINUS, i + 1).data()) && context->models[context->axes_ex[axis_i].model_edit_i].points[i].y > 0) {  // Button to decrease the y-value of the point
                    context->models[context->axes_ex[axis_i].model_edit_i].points[i].y--;
                    update_model = true;
                }
                ImGui::SameLine();
                if (ImGui::Button(fmt::format("{}##YP{}", ICON_FA_PLUS, i + 1).data()) && context->models[context->axes_ex[axis_i].model_edit_i].points[i].y < 100) {  // Button to increase the y-value of the point
                    context->models[context->axes_ex[axis_i].model_edit_i].points[i].y++;
                    update_model = true;
                }
                ImGui::SameLine();
                ImGui::SameLine();
                if (ImGui::SliderInt(fmt::format("Y##{}", i + 1).data(), &context->models[context->axes_ex[axis_i].model_edit_i].points[i].y, 0, 100)) update_model = true;  // Slider for adjusting the y-value of the point
            }
ImGui::PopItemWidth(); // Pop the item width setting

if (update_model) {
    std::array<glm::vec2, 6> model;
    for (int i = 0; i < context->models[context->axes_ex[axis_i].model_edit_i].points.size(); i++) {
        model[i] = {
            static_cast<float>(context->models[context->axes_ex[axis_i].model_edit_i].points[i].x) / 100.f,
            static_cast<float>(context->models[context->axes_ex[axis_i].model_edit_i].points[i].y) / 100.f
        };
    }
    const auto err = context->handle->set_bezier_model(context->axes_ex[axis_i].model_edit_i, model);
    if (err) spdlog::warn(*err);
    else spdlog::info("Model updated.");
}

if (context->axes[axis_i].curve_i != context->axes_ex[axis_i].model_edit_i) {
    const auto err = context->handle->set_axis_bezier_index(axis_i, context->axes_ex[axis_i].model_edit_i);
    if (err) spdlog::warn(*err);
    else spdlog::info("Axis model index updated.");
}

// Function: emit_content_device_panel
animation_under_construction.play = false; // Set the play flag of the "animation_under_construction" to false
animation_under_construction.playing = false; // Set the playing flag of the "animation_under_construction" to false

if (ImGui::BeginTabBar("##AppModeBar")) { // Begin a tab bar with the identifier "##AppModeBar"
    if (ImGui::BeginTabItem(fmt::format("{} Account", ICON_FA_USER).data())) { // Begin a tab item with the label " Account" and icon
        static bool logged_in = false; // Declare a static boolean variable "logged_in" and set it to false
        const auto logged_in_rn = !account_session_token || account_creation_response || account_login_response || account_confirmation_response || account_pw_reset_response || account_pw_reset_confirm_response; // Check if the user is logged in
        if (logged_in_rn != logged_in) { // If the login state has changed
            spdlog::debug("Login state change: Saving"); // Log a debug message
            cfg_save(); // Save the configuration
            logged_in = logged_in_rn; // Update the logged_in variable
        }
        if (logged_in_rn) { // If the user is logged in
            ImPenUtility pen; // Create an ImPenUtility object named "pen"
            pen.CalculateWindowBounds(); // Calculate the window bounds using the "pen" object
            ImGui::SetCursorPos(pen.GetCenteredPosition({ 300, 400 })); // Set the cursor position using the centered position from the "pen" object
            if (ImGui::BeginChild("AccountEnablementChild", { 300, 400 }, true, ImGuiWindowFlags_NoScrollbar)) { // Begin a child window with the identifier "AccountEnablementChild" and size { 300, 400 }
                if (ImGui::BeginTabBar("##AccountEnablementTabBar")) { // Begin a tab bar with the identifier "##AccountEnablementTabBar"
                    auto flags = (account_creation_response || account_login_response || account_confirmation_response || account_pw_reset_response || account_pw_reset_confirm_response) ? ImGuiInputTextFlags_ReadOnly : 0; // Set flags based on account responses
                    if (ImGui::BeginTabItem("Login")) { // Begin a tab item with the label "Login"
                        if (account_login_response) { // If there is an account login response
                            animation_scan.play = true; // Set the play flag of the "animation_scan" to true
                            ImPenUtility pen; // Create an ImPenUtility object named "pen"
                            pen.CalculateWindowBounds(); // Calculate the window bounds using the "pen" object
                            const auto image_pos = pen.GetCenteredPosition(GLMD_IM2(animation_scan.frames[animation_scan.frame_i]->size)); // Calculate the centered position of the image using the "pen" object
                            ImGui::SetCursorScreenPos(image_pos); // Set the cursor screen position using the image position
                            if (animation_scan.frames.size()) { // If there are animation frames
                                ImGui::Image( // Display an image
                                    reinterpret_cast<ImTextureID>(animation_scan.frames[animation_scan.frame_i]->handle), // Image texture ID
                                    GLMD_IM2(animation_scan.frames[animation_scan.frame_i]->size), // Image size
                                    { 0, 0 }, // UV min
                                    { 1, 1 }, // UV max
                                    { 1, 1, 1, animation_scan.playing ? 1 : 0.8f } // Tint color
                                );
                            }
                            if (account_login_response->valid()) { // If the account login response is valid
                                if (const auto status = account_login_response->wait_for(std::chrono::seconds(0)); status == std::future_status::ready) { // Check the status of the account login response future
                                    const auto response = account_login_response->get(); // Get the account login response value
                                    if (response.has_value()) { // If the response has a value
                                        if (response->value("error", false)) { // If there is an error in the response
                                            account_login_error = response->value("message", "Unknown error."); // Set the login error message
                                            account_session_token.reset(); // Reset the account session token
                                            account_person_name.reset(); // Reset the account person name
                                            account_email.reset(); // Reset the account email
                                        } else if (!account_session_token) { // If there is no account session token
                                            if (response->find("session_token") != response->end()) { // Check if the session token is present in the response
                                                account_session_token = response.value()["session_token"]; // Set the account session token
                                                account_person_name = response.value()["name"]; // Set the account person name
                                                if (account_remember_me) { // If remember me is checked
                                                    cfg["session_email"] = account_email_input_buffer.data(); // Set the session email in the configuration
                                                    cfg["session_token"] = *account_session_token; // Set the session token in the configuration
                                                    cfg["session_person_name"] = *account_person_name; // Set the session person name in the configuration
                                                }
                                                account_login_error.reset(); // Reset the login error message
                                            } else account_login_error = "No session token received."; // Set the login error message
                                        }
                                    } else {
                                        spdlog::error("Unable to login: {}", response.error()); // Log an error message
                                        account_session_token.reset(); // Reset the account session token
                                        account_login_error = response.error(); // Set the login error message
                                    }
                                    account_login_response.reset(); // Reset the account login response
                                }
                            }
                        } else {
                            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x); // Push the item width using the available content region width
                            ImGui::InputTextWithHint("##AccountEnablementEmailInput", "email", account_email_input_buffer.data(), account_email_input_buffer.size(), flags); // Display an input text box for email with a hint
                            ImGui::InputTextWithHint("##AccountEnablementPasswordInput", "password", account_password_input_buffer.data(), account_password_input_buffer.size(), flags | ImGuiInputTextFlags_Password); // Display an input text box for password with a hint and password input flags
                            ImGui::PopItemWidth(); // Pop the item width setting
                            if (ImGui::Button("Login", { ImGui::GetContentRegionAvail().x, 0 })) { // Display a button with the label "Login"
                                spdlog::debug("Starting login attempt now."); // Log a debug message
                                account_login_response = api::customer::get_session_token( // Call the API to get the account session token
                                    account_email_input_buffer.data(), // Pass the email input buffer as the email parameter
                                    account_password_input_buffer.data() // Pass the password input buffer as the password parameter
                                );
                                animation_scan.frame_i = 0; // Reset the frame index of the scan animation
                            }
                            if (ImGui::Checkbox("Remember me", &account_remember_me)); // Display a checkbox for remember me
                            if (account_login_error) ImGui::TextColored({ 192.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f }, account_login_error->data()); // Display the login error message with colored text
                        }
                        ImGui::EndTabItem(); // End the tab item
                    }
                }
                ImGui::EndChild(); // End the child window
            }
        }
        ImGui::EndChild(); // End the child window
    }

    static void emit_content_device_panel() {
        // Function: emit_content_device_panel
        animation_under_construction.play = false; // Set the play flag of the "animation_under_construction" to false
        animation_under_construction.playing = false; // Set the playing flag of the "animation_under_construction" to false

        if (ImGui::BeginTabBar("##AppModeBar")) { // Begin a tab bar with the identifier "##AppModeBar"
            if (ImGui::BeginTabItem(fmt::format("{} Account", ICON_FA_USER).data())) { // Begin a tab item with the label " Account" and icon
                static bool logged_in = false; // Declare a static boolean variable "logged_in" and set it to false
                const auto logged_in_rn = !account_session_token || account_creation_response || account_login_response || account_confirmation_response || account_pw_reset_response || account_pw_reset_confirm_response; // Check if the user is logged in
                if (logged_in_rn != logged_in) { // If the login state has changed
                    spdlog::debug("Login state change: Saving"); // Log a debug message
                    cfg_save(); // Save the configuration
                    logged_in = logged_in_rn; // Update the logged_in variable
                }
                if (logged_in_rn) { // If the user is logged in
                    ImPenUtility pen; // Create an ImPenUtility object named "pen"
                    pen.CalculateWindowBounds(); // Calculate the window bounds using the "pen" object
                    ImGui::SetCursorPos(pen.GetCenteredPosition({ 300, 400 })); // Set the cursor position using the centered position from the "pen" object
                    if (ImGui::BeginChild("AccountEnablementChild", { 300, 400 }, true, ImGuiWindowFlags_NoScrollbar)) { // Begin a child window with the identifier "AccountEnablementChild" and size { 300, 400 }
                        if (ImGui::BeginTabBar("##AccountEnablementTabBar")) { // Begin a tab bar with the identifier "##AccountEnablementTabBar"
                            auto flags = (account_creation_response || account_login_response || account_confirmation_response || account_pw_reset_response || account_pw_reset_confirm_response) ? ImGuiInputTextFlags_ReadOnly : 0; // Set flags based on account responses
                            if (ImGui::BeginTabItem("Login")) { // Begin a tab item with the label "Login"
                                if (account_login_response) { // If there is an account login response
                                    animation_scan.play = true; // Set the play flag of the "animation_scan" to true
                                    ImPenUtility pen; // Create an ImPenUtility object named "pen"
                                    pen.CalculateWindowBounds(); // Calculate the window bounds using the "pen" object
                                    const auto image_pos = pen.GetCenteredPosition(GLMD_IM2(animation_scan.frames[animation_scan.frame_i]->size)); // Calculate the centered position of the image using the "pen" object
                                    ImGui::SetCursorScreenPos(image_pos); // Set the cursor screen position using the image position
                                    if (animation_scan.frames.size()) { // If there are animation frames
                                        ImGui::Image( // Display an image
                                            reinterpret_cast<ImTextureID>(animation_scan.frames[animation_scan.frame_i]->handle), // Image texture ID
                                            GLMD_IM2(animation_scan.frames[animation_scan.frame_i]->size), // Image size
                                            { 0, 0 }, // UV min
                                            { 1, 1 }, // UV max
                                            { 1, 1, 1, animation_scan.playing ? 1 : 0.8f } // Tint color
                                        );
                                    }
                                    if (account_login_response->valid()) { // If the account login response is valid
                                        if (const auto status = account_login_response->wait_for(std::chrono::seconds(0)); status == std::future_status::ready) { // Check the status of the account login response future
                                            const auto response = account_login_response->get(); // Get the account login response value
                                            if (response.has_value()) { // If the response has a value
                                                if (response->value("error", false)) { // If there is an error in the response
                                                    account_login_error = response->value("message", "Unknown error."); // Set the login error message
                                                    account_session_token.reset(); // Reset the account session token
                                                    account_person_name.reset(); // Reset the account person name
                                                    account_email.reset(); // Reset the account email
                                                } else if (!account_session_token) { // If there is no account session token
                                                    if (response->find("session_token") != response->end()) { // Check if the session token is present in the response
                                                        account_session_token = response.value()["session_token"]; // Set the account session token
                                                        account_person_name = response.value()["name"]; // Set the account person name
                                                        if (account_remember_me) { // If remember me is checked
                                                            cfg["session_email"] = account_email_input_buffer.data(); // Set the session email in the configuration
                                                            cfg["session_token"] = *account_session_token; // Set the session token in the configuration
                                                            cfg["session_person_name"] = *account_person_name; // Set the session person name in the configuration
                                                        }
                                                        account_login_error.reset(); // Reset the login error message
                                                    } else account_login_error = "No session token received."; // Set the login error message
                                                }
                                            } else {
                                                spdlog::error("Unable to login: {}", response.error()); // Log an error message
                                                account_session_token.reset(); // Reset the account session token
                                                account_login_error = response.error(); // Set the login error message
                                            }
                                            account_login_response.reset(); // Reset the account login response
                                        }
                                    }
                                } else {
                                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x); // Push the item width using the available content region width
                                    ImGui::InputTextWithHint("##AccountEnablementEmailInput", "email", account_email_input_buffer.data(), account_email_input_buffer.size(), flags); // Display an input text box for email with a hint
                                    ImGui::InputTextWithHint("##AccountEnablementPasswordInput", "password", account_password_input_buffer.data(), account_password_input_buffer.size(), flags | ImGuiInputTextFlags_Password); // Display an input text box for password with a hint and password input flags
                                    ImGui::PopItemWidth(); // Pop the item width setting
                                    if (ImGui::Button("Login", { ImGui::GetContentRegionAvail().x, 0 })) { // Display a button with the label "Login"
                                        spdlog::debug("Starting login attempt now."); // Log a debug message
                                        account_login_response = api::customer::get_session_token( // Call the API to get the account session token
                                            account_email_input_buffer.data(), // Pass the email input buffer as the email parameter
                                            account_password_input_buffer.data() // Pass the password input buffer as the password parameter
                                        );
                                        animation_scan.frame_i = 0; // Reset the frame index of the scan animation
                                    }
                                    if (ImGui::Checkbox("Remember me", &account_remember_me)); // Display a checkbox for remember me
                                    if (account_login_error) ImGui::TextColored({ 192.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f }, account_login_error->data()); // Display the login error message with colored text
                                }
                                ImGui::EndTabItem(); // End the tab item
                            } // End the tab item "Login"
if (ImGui::BeginTabItem("Create Account")) { // Begin a tab item with the label "Create Account"
    if (account_creation_response || account_confirmation_response) { // If there is an account creation response or account confirmation response
        animation_scan.play = true; // Set the play flag of the "animation_scan" to true
        ImPenUtility pen; // Create an ImPenUtility object named "pen"
        pen.CalculateWindowBounds(); // Calculate the window bounds using the "pen" object
        const auto image_pos = pen.GetCenteredPosition(GLMD_IM2(animation_scan.frames[animation_scan.frame_i]->size)); // Calculate the centered position of the image using the "pen" object
        ImGui::SetCursorScreenPos(image_pos); // Set the cursor screen position using the image position
        if (animation_scan.frames.size()) { // If there are animation frames
            ImGui::Image( // Display an image
                reinterpret_cast<ImTextureID>(animation_scan.frames[animation_scan.frame_i]->handle), // Image texture ID
                GLMD_IM2(animation_scan.frames[animation_scan.frame_i]->size), // Image size
                { 0, 0 }, // UV min
                { 1, 1 }, // UV max
                { 1, 1, 1, animation_scan.playing ? 1 : 0.8f } // Tint color
            );
        }
        if (account_creation_response && account_creation_response->valid()) { // If there is an account creation response and it is valid
            if (const auto status = account_creation_response->wait_for(std::chrono::seconds(0)); status == std::future_status::ready) { // Check the status of the account creation response future
                const auto response = account_creation_response->get(); // Get the account creation response value
                if (response.has_value()) { // If the response has a value
                    if (response->value("error", false)) account_creation_error = response->value("message", "Unknown error."); // If there is an error in the response, set the creation error message
                    else {
                        if (response->find("session_token") != response->end()) { // Check if the session token is present in the response
                            account_session_token = response.value()["session_token"]; // Set the account session token
                            account_person_name = response.value()["name"]; // Set the account person name
                            if (account_remember_me) { // If remember me is checked
                                cfg["session_email"] = account_email_input_buffer.data(); // Set the session email in the configuration
                                cfg["session_token"] = *account_session_token; // Set the session token in the configuration
                                cfg["session_person_name"] = *account_person_name; // Set the session person name in the configuration
                            }
                            account_creation_error.reset(); // Reset the creation error message
                        } else {
                            account_creation_error = response->value("message", "An error occurred."); // Set the creation error message
                            account_activation_awaits = true; // Set the account activation awaits flag
                        }
                        
                    }
                } else {
                    spdlog::error("Unable to create account: {}", response.error()); // Log an error message
                    account_creation_error = response.error(); // Set the creation error message
                }
                account_creation_response.reset(); // Reset the account creation response
            }
        }
        if (account_confirmation_response && account_confirmation_response->valid()) { // If there is an account confirmation response and it is valid
            if (const auto status = account_confirmation_response->wait_for(std::chrono::seconds(0)); status == std::future_status::ready) { // Check the status of the account confirmation response future
                const auto response = account_confirmation_response->get(); // Get the account confirmation response value
                if (response.has_value()) { // If the response has a value
                    account_confirmation_error = response->value("message", "An error occurred."); // Set the confirmation error message
                } else {
                    spdlog::error("Unable to activate account: {}", response.error()); // Log an error message
                    account_confirmation_error = response.error(); // Set the confirmation error message
                }
                account_confirmation_response.reset(); // Reset the account confirmation response
            }
        }
    } else {
        if (account_activation_awaits) { // If account activation is awaited
            ImGui::TextWrapped("Enter the account activation code that was sent to your email address. If it doesn't arrive, try checking your spam folder."); // Display a wrapped text
            ImGui::Separator(); // Display a separator
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x); // Push the item width using the available content region width
            ImGui::InputTextWithHint("##AccountEnablementEmailInput", "e-mail", account_email_input_buffer.data(), account_email_input_buffer.size(), flags); // Display an input text box for email with a hint
            ImGui::InputTextWithHint("##AccountEnablementPinInput", "code", account_code_input_buffer.data(), account_code_input_buffer.size(), flags); // Display an input text box for activation code with a hint
            ImGui::PopItemWidth(); // Pop the item width setting
            if (ImGui::Button("Activate Account", { ImGui::GetContentRegionAvail().x, 0 })) { // Display a button with the label "Activate Account"
                spdlog::debug("Starting account activation attempt now."); // Log a debug message
                account_confirmation_response = api::customer::activate_account( // Call the API to activate the account
                    account_email_input_buffer.data(), // Pass the email input buffer as the email parameter
                    account_code_input_buffer.data() // Pass the activation code input buffer as the code parameter
                );
                animation_scan.frame_i = 0; // Reset the frame index of the scan animation
            }
            if (ImGui::Button("Request Activation Code", { ImGui::GetContentRegionAvail().x, 0 })) { // Display a button with the label "Request Activation Code"
                account_activation_awaits = false; // Set the account activation awaits flag to false
                account_confirmation_error.reset(); // Reset the confirmation error message
                account_creation_error.reset(); // Reset the creation error message
            }
        } else {
            ImGui::TextWrapped("You can create an account here. An email with an activation code in it will be sent you. You must provide it on the next page in order to activate your account. If it doesn't arrive, try checking your spam folder."); // Display a wrapped text
            ImGui::Separator(); // Display a separator
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x); // Push the item width using the available content region width
            ImGui::InputTextWithHint("##AccountEnablementNameInput", "name", account_name_input_buffer.data(), account_name_input_buffer.size(), flags); // Display an input text box for name with a hint
            ImGui::InputTextWithHint("##AccountEnablementEmailInput", "e-mail", account_email_input_buffer.data(), account_email_input_buffer.size(), flags); // Display an input text box for email with a hint
            ImGui::InputTextWithHint("##AccountEnablementPasswordInput", "password", account_password_input_buffer.data(), account_password_input_buffer.size(), flags | ImGuiInputTextFlags_Password); // Display an input text box for password with a hint and password mask
            ImGui::PopItemWidth(); // Pop the item width setting
            if (ImGui::Button("Create Account", { ImGui::GetContentRegionAvail().x, 0 })) { // Display a button with the label "Create Account"
                spdlog::debug("Starting account creation attempt now."); // Log a debug message
                account_creation_response = api::customer::create_new( // Call the API to create a new account
                    account_email_input_buffer.data(), // Pass the email input buffer as the email parameter
                    account_name_input_buffer.data(), // Pass the name input buffer as the name parameter
                    account_password_input_buffer.data() // Pass the password input buffer as the password parameter
                );
                animation_scan.frame_i = 0; // Reset the frame index of the scan animation
            }
        }
    }
if (ImGui::Button("Use Activation Code", { ImGui::GetContentRegionAvail().x, 0 })) { // If "Use Activation Code" button is pressed
    account_activation_awaits = true; // Set the account activation awaits flag to true
    account_confirmation_error.reset(); // Reset the account confirmation error
    account_creation_error.reset(); // Reset the account creation error
    account_code_input_buffer.fill(0); // Fill the account code input buffer with zeros
}

if (account_creation_error) // If there is an account creation error
    ImGui::TextColored({ 192.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f }, account_creation_error->data()); // Display the account creation error message in red

if (account_confirmation_error) // If there is an account confirmation error
    ImGui::TextColored({ 192.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f }, account_confirmation_error->data()); // Display the account confirmation error message in red

ImGui::EndTabItem(); // End the current tab

/*
if (ImGui::BeginTabItem("Activate")) {
    // Code for the "Activate" tab goes here
}
*/

if (ImGui::BeginTabItem("Reset Password")) { // Begin the "Reset Password" tab
    if (account_pw_reset_response || account_pw_reset_confirm_response) { // If there is a password reset response or password reset confirm response
        animation_scan.play = true; // Set the animation scan to play
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
        if (account_pw_reset_response && account_pw_reset_response->valid()) { // If there is a valid password reset response
            if (const auto status = account_pw_reset_response->wait_for(std::chrono::seconds(0)); status == std::future_status::ready) {
                const auto response = account_pw_reset_response->get();
                if (response.has_value()) {
                    account_pw_reset_error = response->value("message", "An error occurred."); // Set the password reset error message
                    if (!response->value("error", true)) {
                        account_pw_reset_awaits = true; // Set the password reset awaits flag to true
                        account_password_input_buffer.fill(0); // Fill the account password input buffer with zeros
                        account_code_input_buffer.fill(0); // Fill the account code input buffer with zeros
                    }
                } else {
                    spdlog::error("Unable to reset password: {}", response.error()); // Log the error message
                    account_pw_reset_error = response.error(); // Set the password reset error message
                }
                account_pw_reset_response.reset(); // Reset the password reset response
            }
        }
        if (account_pw_reset_confirm_response && account_pw_reset_confirm_response->valid()) { // If there is a valid password reset confirm response
            if (const auto status = account_pw_reset_confirm_response->wait_for(std::chrono::seconds(0)); status == std::future_status::ready) {
                const auto response = account_pw_reset_confirm_response->get();
                if (response.has_value()) {
                    account_pw_reset_confirm_error = response->value("message", "An error occurred."); // Set the password reset confirm error message
                } else {
                    spdlog::error("Unable to reset password: {}", response.error()); // Log the error message
                    account_pw_reset_confirm_error = response.error(); // Set the password reset confirm error message
                }
                account_pw_reset_confirm_response.reset(); // Reset the password reset confirm response
            }
        }
    } else {
        if (account_pw_reset_awaits) { // If the password reset is awaited
            ImGui::TextWrapped("Enter the password reset code that was sent to your email address. If it doesn't arrive, try checking your spam folder.");
            ImGui::Separator();
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint("##AccountEnablementEmailInput", "e-mail", account_email_input_buffer.data(), account_email_input_buffer.size(), flags);
            ImGui::InputTextWithHint("##AccountEnablementPinInput", "code", account_code_input_buffer.data(), account_code_input_buffer.size(), flags);
            ImGui::InputTextWithHint("##AccountEnablementPasswordInput", "password", account_password_input_buffer.data(), account_password_input_buffer.size(), flags | ImGuiInputTextFlags_Password);
            ImGui::PopItemWidth();
            if (ImGui::Button("Submit", { ImGui::GetContentRegionAvail().x, 0 })) {
                account_pw_reset_confirm_response = api::customer::password_reset(
                    account_email_input_buffer.data(),
                    account_code_input_buffer.data(),
                    account_password_input_buffer.data()
                );
            }
            if (ImGui::Button("Request New Code", { ImGui::GetContentRegionAvail().x, 0 })) {
                account_pw_reset_awaits = false; // Set the password reset awaits flag to false
                account_pw_reset_error.reset(); // Reset the password reset error
                account_pw_reset_confirm_error.reset(); // Reset the password reset confirm error
            }
        } else {
            ImGui::TextWrapped("You can reset your password here. An email with a reset code in it will be sent you. You must provide it on the next page in order to reset your password. If it doesn't arrive, try checking your spam folder.");
            ImGui::Separator();
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputTextWithHint("##AccountEnablementEmailInput", "e-mail", account_email_input_buffer.data(), account_email_input_buffer.size(), flags);
            ImGui::PopItemWidth();
            if (ImGui::Button("Send Reset Code", { ImGui::GetContentRegionAvail().x, 0 })) {
                account_pw_reset_response = api::customer::request_password_reset(account_email_input_buffer.data());
            }
            if (ImGui::Button("Use Existing Code", { ImGui::GetContentRegionAvail().x, 0 })) {
                account_pw_reset_awaits = true; // Set the password reset awaits flag to true
                account_pw_reset_error.reset(); // Reset the password reset error
                account_pw_reset_confirm_error.reset(); // Reset the password reset confirm error
            }
        }

        if (account_pw_reset_error) // If there is a password reset error
            ImGui::TextColored({ 192.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f }, account_pw_reset_error->data()); // Display the password reset error message in red

        if (account_pw_reset_confirm_error) // If there is a password reset confirm error
            ImGui::TextColored({ 192.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f }, account_pw_reset_confirm_error->data()); // Display the password reset confirm error message in red
    }
    ImGui::EndTabItem(); // End the "Reset Password" tab
}

ImGui::EndTabBar(); // End the tab bar
  }
} // End of the outermost if statement

ImGui::EndChild(); // End of the child window

else {
  ImGui::TextColored({ .2f, 1, .2f, 1 }, fmt::format("{} Logged In", ICON_FA_CHECK_DOUBLE).data()); // Display the "Logged In" text in colored format
  ImGui::SameLine(); // Move the cursor to the same line
  ImGui::TextDisabled(fmt::format("({}, Token: {})", *account_person_name, *account_session_token).data()); // Display the person's name and session token in disabled format
  animation_under_construction.playing = true; // Set the playing flag of the animation_under_construction to true
  animation_under_construction.loop = true; // Set the loop flag of the animation_under_construction to true
  ImPenUtility pen; // Create an ImPenUtility object
  pen.CalculateWindowBounds(); // Calculate the window bounds
  const auto image_pos = pen.GetCenteredPosition(GLMD_IM2(animation_under_construction.frames[animation_under_construction.frame_i]->size)); // Calculate the centered position of the image
  ImGui::SetCursorScreenPos(image_pos); // Set the cursor position to the image position

  if (animation_under_construction.frames.size()) { // Check if there are frames in animation_under_construction
    ImGui::Image(
      reinterpret_cast<ImTextureID>(animation_under_construction.frames[animation_under_construction.frame_i]->handle), // Display the current frame of the animation_under_construction
      GLMD_IM2(animation_under_construction.frames[animation_under_construction.frame_i]->size)
    );
  }
}

ImGui::EndTabItem(); // End the current tab item

if (ImGui::BeginTabItem(fmt::format("{} Hardware", ICON_FA_TOOLS).data())) { // Begin a new tab item with the label "Hardware" and the "Tools" icon
  enum class selection_type {
    axis,
    button,
    hat
  }; // Define an enumeration class with three possible values: axis, button, hat

  if (device_contexts.size() || enable_legacy_support) { // Check if the device_contexts vector is non-empty or the enable_legacy_support flag is true
    animation_scan.playing = false; // Set the playing flag of the animation_scan to false

    if (ImGui::BeginTabBar("##DeviceTabBar")) { // Begin a tab bar with the ID "##DeviceTabBar"
      for (const auto &context : device_contexts) { // Iterate over each context in the device_contexts vector
        std::lock_guard guard(context->mutex); // Create a lock guard for the context's mutex

        if (ImGui::BeginTabItem(fmt::format("{} {}##{}", ICON_FA_MICROCHIP, context->name, context->serial).data())) { // Begin a new tab item with the label containing the icon, context name, and serial number
          if (context->handle) { // Check if the context has a handle
            ImGui::TextColored({ .2f, 1, .2f, 1 }, fmt::format("{} Connected", ICON_FA_CHECK_DOUBLE).data()); // Display the "Connected" text in colored format
            ImGui::SameLine(); // Move the cursor to the same line
            ImGui::TextDisabled(fmt::format("v{}.{}.{}", context->version_major, context->version_minor, context->version_revision).data()); // Display the version information in disabled format
          } else {
            ImGui::TextColored({ 1, 1, .2f, 1 }, fmt::format("{} Disconnected", ICON_FA_SPINNER).data()); // Display the "Disconnected" text in colored format
          }

          if (context->initial_communication_complete) { // Check if the initial communication with the context is complete
            const auto top_y = ImGui::GetCursorScreenPos().y; // Get the top y-coordinate of the current position
            animation_comm.playing = false; // Set the playing flag of the animation_comm to false

            if (ImGui::BeginChild("##DeviceInteractionBox", { 200, 86 }, true, ImGuiWindowFlags_MenuBar)) { // Begin a child window with the ID "##DeviceInteractionBox" and a size of {200, 86} pixels, along with a menubar
              if (ImGui::BeginMenuBar()) { // Begin the menubar
                ImGui::Text(fmt::format("{} Controls", ICON_FA_SATELLITE_DISH).data()); // Display the "Controls" text in the menubar
                ImGui::EndMenuBar(); // End the menubar
              }

              if (ImGui::Button(fmt::format("{} Save to Chip", ICON_FA_FILE_IMPORT).data(), { ImGui::GetContentRegionAvail().x, 0 })) { // Create a button with the label "Save to Chip" and the "File Import" icon
                const auto err = context->handle->commit(); // Commit the changes to the context's handle
                if (err) spdlog::error(*err); // Display an error message if there is an error
                else spdlog::info("Settings saved."); // Display a success message if the settings are saved
              }

              if (ImGui::Button(fmt::format("{} Clear Chip", ICON_FA_ERASER).data(), { ImGui::GetContentRegionAvail().x, 0 })) { // Create a button with the label "Clear Chip" and the "Eraser" icon
                // Perform some action when the button is clicked
              }
            }

            ImGui::EndChild(); // End the child window

            static int current_selection; // Define a static integer variable to hold the current selection

            if (ImGui::BeginChild("##DeviceHardwareList", { 200, 0 }, true, ImGuiWindowFlags_MenuBar)) { // Begin a child window with the ID "##DeviceHardwareList" and a dynamic height, along with a menubar
              if (ImGui::BeginMenuBar()) { // Begin the menubar
                ImGui::Text(fmt::format("{} Inputs", ICON_FA_SITEMAP).data()); // Display the "Inputs" text in the menubar
                ImGui::EndMenuBar(); // End the menubar
              }

              const auto num_axes = context->axes.size(); // Get the number of axes in the context

              if (num_axes) { // Check if there are any axes
                for (int i = 0; i < num_axes; i++) { // Iterate over each axis
                  if (current_selection != i) { // Check if the current selection is not equal to the current axis
                    ImGui::PushStyleColor(ImGuiCol_Button, { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .2f }); // Push a new style color for the button
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .3f }); // Push a new style color for the active button
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .4f }); // Push a new style color for the hovered button
                  } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, { 24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f }); // Push a new style color for the button
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f }); // Push a new style color for the active button
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f }); // Push a new style color for the hovered button
                  }

                  const auto label = (i == 0 ? "Throttle" : (i == 1 ? "Brake" : (i == 2 ? "Clutch" : "?"))); // Set the label based on the current axis

                  if (ImGui::Button(label, { ImGui::GetContentRegionAvail().x, 40 })) { // Create a button with the label and a size of {content_region_width, 40}
                    current_selection = i; // Set the current selection to the current axis
                  }

                  ImGui::PopStyleColor(3); // Pop the style colors
                }
              }
            }

            ImGui::EndChild(); // End the child window
            ImGui::SameLine(0, ImGui::GetStyle().FramePadding.x); // Move the cursor to the same line with frame padding
            ImGui::SetCursorScreenPos({ ImGui::GetCursorScreenPos().x, top_y }); // Set the cursor position to the top y-coordinate

            emit_axis_profile_slice(context, current_selection); // Call the emit_axis_profile_slice function with the context and current_selection arguments
          } else {
            if (!animation_comm.playing) animation_comm.time = 164.0 / animation_comm.frame_rate; // Set the time of animation_comm if it is not playing
            animation_comm.playing = true; // Set the playing flag of the animation_comm to true
            ImPenUtility pen; // Create an ImPenUtility object
            pen.CalculateWindowBounds(); // Calculate the window bounds
            const auto image_pos = pen.GetCenteredPosition(GLMD_IM2(animation_comm.frames[animation_comm.frame_i]->size)); // Calculate the centered position of the image
            ImGui::SetCursorScreenPos(image_pos); // Set the cursor position to the image position

            if (animation_comm.frames.size()) { // Check if there are frames in animation_comm
              ImGui::Image(
                  reinterpret_cast<ImTextureID>(animation_comm.frames[animation_comm.frame_i]->handle), // Display the current frame of the animation_comm
                  GLMD_IM2(animation_comm.frames[animation_comm.frame_i]->size)
              );
            }
          }

          ImGui::EndTabItem(); // End the current tab item
        }
	}
  if (enable_legacy_support && ImGui::BeginTabItem(fmt::format("{} Virtual Pedals", ICON_FA_GHOST).data())) { // Check if legacy support is enabled and begin a new tab with the label "Virtual Pedals" and the "Ghost" icon
    if (legacy::present()) { // Check if legacy is present
      ImGui::TextColored({ .2f, 1, .2f, 1 }, fmt::format("{} Online", ICON_FA_CHECK_DOUBLE).data()); // Display the "Online" text in colored format
    } else {
      ImGui::TextColored({ .2f, 1, .2f, 1 }, fmt::format("{} Ready", ICON_FA_CHECK).data()); // Display the "Ready" text in colored format
      ImGui::SameLine();
      ImGui::TextDisabled("No hardware detected."); // Display the "No hardware detected." text in disabled format
    }
    
    const auto top_y = ImGui::GetCursorScreenPos().y; // Get the top y-coordinate of the current position

    if (ImGui::BeginChild("##DeviceInteractionBox", { 200, 86 }, true, ImGuiWindowFlags_MenuBar)) { // Begin a child window with the ID "##DeviceInteractionBox" and a size of {200, 86} pixels, along with a menubar
      if (ImGui::BeginMenuBar()) { // Begin the menubar
        ImGui::Text(fmt::format("{} Controls", ICON_FA_SATELLITE_DISH).data()); // Display the "Controls" text in the menubar
        ImGui::EndMenuBar(); // End the menubar
      }

      if (ImGui::Button(fmt::format("{} Save Settings", ICON_FA_FILE_IMPORT).data(), { ImGui::GetContentRegionAvail().x, 0 })) { // Create a button with the label "Save Settings" and the "File Import" icon
        if (const auto err = legacy::save_settings(); err) { // Save the settings and check if there is an error
          spdlog::error("Unable to save settings: {}", *err); // Display an error message if there is an error
        } else {
          spdlog::info("Settings saved."); // Display a success message if the settings are saved
        }
      }

      if (ImGui::Button(fmt::format("{} Clear Settings", ICON_FA_ERASER).data(), { ImGui::GetContentRegionAvail().x, 0 })) {
        // Perform some action when the button is clicked
      }
    }

    ImGui::EndChild(); // End the child window

    static int current_selection; // Define a static integer variable to hold the current selection

    if (ImGui::BeginChild("##DeviceHardwareList", { 200, 0 }, true, ImGuiWindowFlags_MenuBar)) { // Begin a child window with the ID "##DeviceHardwareList" and a dynamic height, along with a menubar
      if (ImGui::BeginMenuBar()) { // Begin the menubar
        ImGui::Text(fmt::format("{} Inputs", ICON_FA_SITEMAP).data()); // Display the "Inputs" text in the menubar
        ImGui::EndMenuBar(); // End the menubar
      }

      for (int i = 0; i < legacy::axes.size(); i++) { // Iterate over each axis
        if (current_selection != i) { // Check if the current selection is not equal to the current axis
          ImGui::PushStyleColor(ImGuiCol_Button, { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .2f }); // Push a new style color for the button
          ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .3f }); // Push a new style color for the active button
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 12.f / 255.f, 12.f / 255.f, 12.f / 255.f, .4f }); // Push a new style color for the hovered button
        } else {
          ImGui::PushStyleColor(ImGuiCol_Button, { 24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f }); // Push a new style color for the button
          ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f }); // Push a new style color for the active button
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 24.f / 255.f, 24.f / 255.f, 24.f / 255.f, 1.f }); // Push a new style color for the hovered button
        }

        if (legacy::axes[i].label) { // Check if the axis has a label
          if (ImGui::Button(fmt::format("{}##VirtualAxis{}SelectionButton", legacy::axes[i].label->data(), i).data(), { ImGui::GetContentRegionAvail().x, 40 })) { // Create a button with the label of the axis and a size of {content_region_width, 40}
            current_selection = i; // Set the current selection to the current axis
          }
        } else if (ImGui::Button(fmt::format("Axis #{}##VirtualAxis{}SelectionButton", i, i).data(), { ImGui::GetContentRegionAvail().x, 40 })) { // Create a button with the label "Axis #" and the axis number, and a size of {content_region_width, 40}
          current_selection = i; // Set the current selection to the current axis
        }

        ImGui::PopStyleColor(3); // Pop the style colors

        ImGui::ProgressBar(legacy::axes[i].output, { ImGui::GetContentRegionAvail().x, 8 }, ""); // Display a progress bar for the output of the axis
      }
    }

    ImGui::EndChild(); // End the child window

    ImGui::SameLine(0, ImGui::GetStyle().FramePadding.x); // Move the cursor to the same line with frame padding
    ImGui::SetCursorScreenPos({ ImGui::GetCursorScreenPos().x, top_y }); // Set the cursor position to the top y-coordinate

    if (ImGui::BeginChild(fmt::format("##VirtualAxis#{}_Window", current_selection).data(), { 0, 0 }, true, ImGuiWindowFlags_MenuBar)) { // Begin a child window with the ID "##VirtualAxis#{}_Window" and a dynamic size, along with a menubar
      if (ImGui::BeginMenuBar()) { // Begin the menubar
        ImGui::Text(fmt::format("{} Axis #{} Configurations", ICON_FA_COGS, current_selection + 1).data()); // Display the axis configurations with the axis number
        ImGui::EndMenuBar(); // End the menubar
      }

      ImGui::SetNextItemWidth(400); // Set the width of the next item to 400 pixels
      ImGui::InputTextWithHint(fmt::format("##VirtualAxis#{}_LabelInput", current_selection).data(), "Label", legacy::axes[current_selection].label_buffer.data(), legacy::axes[current_selection].label_buffer.size()); // Create an input text field for the label of the axis

      ImGui::SameLine(); // Move to the same line
      if (ImGui::Button(fmt::format("Set Label##VirtualAxis#{}_LabelUpdateButton", current_selection).data(), { ImGui::GetContentRegionAvail().x, 0 })) { // Create a button with the label "Set Label"
        if (const auto trimmed = pystring::strip(legacy::axes[current_selection].label_buffer.data(), ""); trimmed != "") { // Trim the label and check if it is not empty
          spdlog::debug("Update label: {}", trimmed); // Output a debug message with the trimmed label
          legacy::axes[current_selection].label = trimmed; // Set the label of the current axis to the trimmed label
        } else {
          legacy::axes[current_selection].label.reset(); // Reset the label of the current axis
        }
      }

      if (ImGui::BeginChild("##{}InputRangeWindow", { 0, 164 }, true, ImGuiWindowFlags_MenuBar)) { // Begin a child window with the ID "##{}InputRangeWindow" and a size of {0, 164} pixels, along with a menubar
        bool update_axis_range = false; // Define a boolean variable to track if the axis range should be updated

        if (ImGui::BeginMenuBar()) { // Begin the menubar
          ImGui::Text(fmt::format("{} Range", ICON_FA_RULER).data()); // Display the "Range" text in the menubar
          ImGui::EndMenuBar(); // End the menubar
        }

        ImGui::ProgressBar(legacy::axes[current_selection].input_raw, { ImGui::GetContentRegionAvail().x, 0 }, fmt::format("{}", legacy::axes[current_selection].input_steps).data()); // Display a progress bar for the raw input of the axis

        ImGui::SameLine(); // Move to the same line
        ImGui::Text("Raw Input"); // Display the "Raw Input" text

        if (ImGui::Button(fmt::format(" {} Set Min ", ICON_FA_ARROW_TO_LEFT).data(), { 100, 0 })) { // Create a button with the label "Set Min" and the "Arrow to Left" icon
          legacy::axes[current_selection].output_steps_min = legacy::axes[current_selection].input_steps; // Set the minimum output steps to the input steps of the axis
          update_axis_range = true; // Set the flag to update the axis range
        }

        if (ImGui::IsItemHovered()) { // Check if the item is being hovered
          ImGui::BeginTooltip(); // Begin a tooltip
          ImGui::Text("Set the minimum range to the current raw input value."); // Display a tooltip text
          ImGui::EndTooltip(); // End the tooltip
        }

        ImGui::SameLine(); // Move to the same line
        ImGui::PushItemWidth(100); // Set the width of the next item to 100 pixels

        if (ImGui::InputInt("Min", &legacy::axes[current_selection].output_steps_min)) { // Create an input field for the minimum output steps
          update_axis_range = true; // Set the flag to update the axis range
        }

        ImGui::SameLine(); // Move to the same line

        if (ImGui::InputInt("Max", &legacy::axes[current_selection].output_steps_max)) { // Create an input field for the maximum output steps
          update_axis_range = true; // Set the flag to update the axis range
        }

        ImGui::PopItemWidth(); // Reset the item width

        ImGui::SameLine(); // Move to the same line

        if (ImGui::Button(fmt::format(" {} Set Max ", ICON_FA_ARROW_TO_RIGHT).data(), { 100, 0 })) { // Create a button with the label "Set Max" and the "Arrow to Right" icon
          legacy::axes[current_selection].output_steps_max = legacy::axes[current_selection].input_steps; // Set the maximum output steps to the input steps of the axis
          update_axis_range = true; // Set the flag to update the axis range
        }
// Check if the item is being hovered and display a tooltip
if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Set the maximum range to the current raw input value.");
    ImGui::EndTooltip();
}

// Slider for the deadzone of the axis, if changed set the flag to update the axis range
if (ImGui::SliderInt("Deadzone", &legacy::axes[current_selection].deadzone, 0, 30, "%d%%"))
    update_axis_range = true;

// Slider for the output limit of the axis, if changed set the flag to update the axis range
if (ImGui::SliderInt("Output Limit##DZH", &legacy::axes[current_selection].output_limit, 50, 100, "%d%%"))
    update_axis_range = true;

// Change the background color of the frame if the axis is not present
if (!legacy::axes[current_selection].present)
    ImGui::PushStyleColor(ImGuiCol_FrameBg, { 72.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f });

// Store the current y position
const auto old_y = ImGui::GetCursorPos().y;

// Add an offset to the y position of the cursor
ImGui::SetCursorPos({ ImGui::GetCursorPos().x, ImGui::GetCursorPos().y + 2 });

// Display the "Signal Slash" icon and align it with the previous position
ImGui::Text(fmt::format("{}", ICON_FA_SIGNAL_SLASH).data());
ImGui::SameLine();
ImGui::SetCursorPos({ ImGui::GetCursorPos().x, old_y });

// Calculate the deadzone padding and the fraction of input steps within the deadzone
const int deadzone_padding = (legacy::axes[current_selection].deadzone / 100.f) * static_cast<float>(legacy::axes[current_selection].output_steps_max - legacy::axes[current_selection].output_steps_min);
const float within_deadzone_fraction = legacy::axes[current_selection].input_steps >= legacy::axes[current_selection].output_steps_min ? (legacy::axes[current_selection].input_steps < legacy::axes[current_selection].output_steps_min + deadzone_padding ? (static_cast<float>(legacy::axes[current_selection].input_steps - legacy::axes[current_selection].output_steps_min) / static_cast<float>((legacy::axes[current_selection].output_steps_min + deadzone_padding) - legacy::axes[current_selection].output_steps_min)) : 1.f) : 0.f;

// Push a style color to change the color of the progress bar
ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 150.f / 255.f, 42.f / 255.f, 42.f / 255.f, 1.f });

// Display a progress bar representing the within_deadzone_fraction
if (legacy::axes[current_selection].deadzone > 0)
    ImGui::ProgressBar(within_deadzone_fraction, { 80, 0 });
else
    ImGui::ProgressBar(0.f, { 80, 0 }, "--");

// Pop the style color
ImGui::PopStyleColor();

// Display the "Signal" icon
ImGui::SameLine();
ImGui::Text(fmt::format("{}", ICON_FA_SIGNAL).data());

// Display a progress bar representing the output of the axis
ImGui::SameLine();
ImGui::PushStyleColor(ImGuiCol_PlotHistogram, { 72.f / 255.f, 150.f / 255.f, 42.f / 255.f, 1.f });
ImGui::ProgressBar(legacy::axes[current_selection].output, { ImGui::GetContentRegionAvail().x, 0 });
ImGui::PopStyleColor();

// If the axis is not present, restore the previous style color and display a tooltip
if (!legacy::axes[current_selection].present) {
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("This axis is not present.");
        ImGui::EndTooltip();
    }
}

// End the child window
ImGui::EndChild();

// Begin the child window for the axis curve settings
if (ImGui::BeginChild(fmt::format("##Axis{}CurveWindow", current_selection).data(), { 0, 0 }, true, ImGuiWindowFlags_MenuBar)) {
    // Display the menu bar with the title
    if (ImGui::BeginMenuBar()) {
        ImGui::Text(fmt::format("{} Curve", ICON_FA_BEZIER_CURVE).data());
        ImGui::EndMenuBar();
    }

    // Get the label for the selected curve model
    const std::string selected_model_label = legacy::axes[current_selection].model_edit_i >= 0 ? (legacy::models[legacy::axes[current_selection].model_edit_i].label ? fmt::format("{} (#{})", *legacy::models[legacy::axes[current_selection].model_edit_i].label, legacy::axes[current_selection].model_edit_i) : fmt::format("Model #{}", legacy::axes[current_selection].model_edit_i)) : "None selected.";

    // Display the combo box for selecting the curve model
    if (ImGui::BeginCombo(fmt::format("##Axis{}CurveOptions", current_selection).data(), selected_model_label.data())) {
        for (int model_i = 0; model_i < legacy::models.size(); model_i++) {
            std::string this_label = legacy::models[model_i].label ? fmt::format("{} (#{})", *legacy::models[model_i].label, model_i) : fmt::format("Model #{}", model_i);
            if (model_i == legacy::axes[current_selection].curve_i)
                this_label += " *";
            if (ImGui::Selectable(this_label.data()))
                legacy::axes[current_selection].model_edit_i = model_i;
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::Text("Curve Model");

    // If a curve model is selected
    if (legacy::axes[current_selection].model_edit_i >= 0) {
        // Display the input text field for the model label
        ImGui::InputText("", legacy::models[legacy::axes[current_selection].model_edit_i].label_buffer.data(), legacy::models[legacy::axes[current_selection].model_edit_i].label_buffer.size());
        ImGui::SameLine();

        // Button to set the label for the model
        if (ImGui::Button("Set Label", { ImGui::GetContentRegionAvail().x, 0 })) {
            legacy::models[legacy::axes[current_selection].model_edit_i].label = legacy::models[legacy::axes[current_selection].model_edit_i].label_buffer.data();
        }

        // Create a vector of glm::dvec2 points for the model
        std::vector<glm::dvec2> model;
        for (auto& percent : legacy::models[legacy::axes[current_selection].model_edit_i].points) {
            model.push_back({
                static_cast<double>(percent.x) / 100.0,
                static_cast<double>(percent.y) / 100.0
            });
        }

        // Calculate the deadzone padding and the CIF (Curve Input Fraction)
        const int deadzone_padding = (legacy::axes[current_selection].deadzone / 100.f) * static_cast<float>(legacy::axes[current_selection].output_steps_max - legacy::axes[current_selection].output_steps_min);
        auto cif = glm::max(0.0, static_cast<double>(legacy::axes[current_selection].input_steps - (legacy::axes[current_selection].output_steps_min + deadzone_padding)) / static_cast<double>(legacy::axes[current_selection].output_steps_max - (legacy::axes[current_selection].output_steps_min + deadzone_padding)));
        if (cif > 1.0)
            cif = 1.0;

        // Plot the cubic curve using the bezier::ui::plot_cubic function
        if (legacy::axes[current_selection].model_edit_i == legacy::axes[current_selection].curve_i) {
            bezier::ui::plot_cubic(model, { 200, 200 }, legacy::axes[current_selection].output, std::nullopt, legacy::axes[current_selection].output_limit / 100.f, cif);
        } else {
            bezier::ui::plot_cubic(model, { 200, 200 }, std::nullopt, std::nullopt, legacy::axes[current_selection].output_limit / 100.f, cif);
        }

        // Begin the child window for the right panel
        ImGui::SameLine();
        if (ImGui::BeginChild(fmt::format("##Axis{}CurveWindowRightPanel", current_selection).data(), { ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y }, false)) {
            ImGui::PushItemWidth(80);

            for (int i = 0; i < legacy::models[legacy::axes[current_selection].model_edit_i].points.size(); i++) {
                // Skip the first and last points
                if (i == 0 || i == legacy::models[legacy::axes[current_selection].model_edit_i].points.size() - 1)
                    continue;

                // Display text labels for specific points
                switch (i) {
                    case 1:
                        ImGui::TextDisabled("20%%");
                        break;
                    case 2:
                        ImGui::TextDisabled("40%%");
                        break;
                    case 3:
                        ImGui::TextDisabled("60%%");
                        break;
                    case 4:
                        ImGui::TextDisabled("80%%");
                        break;
                    default:
                        break;
                }

                ImGui::SameLine();

                // Button to decrease the y value of a point
                if (ImGui::Button(fmt::format("{}##YM{}", ICON_FA_MINUS, i + 1).data()) && legacy::models[legacy::axes[current_selection].model_edit_i].points[i].y > 0) {
                    legacy::models[legacy::axes[current_selection].model_edit_i].points[i].y--;
                }

                ImGui::SameLine();

                // Button to increase the y value of a point
                if (ImGui::Button(fmt::format("{}##YP{}", ICON_FA_PLUS, i + 1).data()) && legacy::models[legacy::axes[current_selection].model_edit_i].points[i].y < 100) {
                    legacy::models[legacy::axes[current_selection].model_edit_i].points[i].y++;
                }

                ImGui::SameLine();

                // Slider to adjust the y value of a point
                if (ImGui::SliderInt(fmt::format("Y##{}", i + 1).data(), &legacy::models[legacy::axes[current_selection].model_edit_i].points[i].y, 0, 100)) {}
            }

            ImGui::PopItemWidth();

            // Update the curve index if it is different from the model index
            if (legacy::axes[current_selection].curve_i != legacy::axes[current_selection].model_edit_i) {
                legacy::axes[current_selection].curve_i = legacy::axes[current_selection].model_edit_i;
            }
        }

        // End the child window for the right panel
        ImGui::EndChild();
    }

    // End the child window for the curve settings
    ImGui::EndChild();
}

// End the child window for the axis settings
ImGui::EndChild();

// End the tab item
ImGui::EndTabItem();

// End the tab bar
ImGui::EndTabBar();
}
  } else {
    // Set animation scan to play
    animation_scan.play = true;

    // Create ImPenUtility object
    ImPenUtility pen;
    // Calculate window bounds
    pen.CalculateWindowBounds();
    // Get the centered position of the image
    const auto image_pos = pen.GetCenteredPosition(GLMD_IM2(animation_scan.frames[animation_scan.frame_i]->size));
    // Set the cursor screen position to the image position
    ImGui::SetCursorScreenPos(image_pos);

    // Check if animation frames exist
    if (animation_scan.frames.size()) {
        // Display the image using ImGui::Image
        ImGui::Image(
            reinterpret_cast<ImTextureID>(animation_scan.frames[animation_scan.frame_i]->handle),
            GLMD_IM2(animation_scan.frames[animation_scan.frame_i]->size),
            { 0, 0 },
            { 1, 1 },
            { 1, 1, 1, animation_scan.playing ? 1 : 0.8f }
        );
    }
}

// End the current tab item
ImGui::EndTabItem();
}

// Commented out code
/*
if (ImGui::BeginTabItem(fmt::format("{} iRacing", ICON_FA_FLAG_CHECKERED).data())) {
    ImGui::Text(fmt::format("Status: {}", magic_enum::enum_name(iracing::get_status())).data());
    ImGui::EndTabItem();
}
*/

// End the tab bar
ImGui::EndTabBar();
}

static void try_toggle_legacy_support() {
    if (enable_legacy_support) {
        // Disable legacy support
        legacy::disable();
        enable_legacy_support = false;
    } else if (const auto err = legacy::enable(); err) {
        // Enable legacy support and handle error if any
        legacy_support_error = true;
        legacy_support_error_description = *err;
    } else {
        enable_legacy_support = true;
    }
}

static void emit_primary_window_menu_bar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu(fmt::format("{} File", ICON_FA_SAVE).data())) {
            if (account_session_token && ImGui::Selectable(fmt::format("{} Logout", ICON_FA_UNDO).data())) {
                // Reset account information and erase session data from config
                account_email.reset();
                account_session_token.reset();
                account_person_name.reset();
                cfg.erase("session_email");
                cfg.erase("session_person_name");
                cfg.erase("session_token");
            }
            if (ImGui::Selectable(fmt::format("{} Save", ICON_FA_SAVE).data())) {
                // Save configuration
                cfg_save();
            }
            // Theme selection option (commented out)
            // if (ImGui::Selectable(fmt::format("{} Theme", ICON_FA_PAINT_ROLLER).data()));

            if (ImGui::Selectable(fmt::format("{} Quit", ICON_FA_SKULL).data())) {
                // Set keep_running to false to exit the program
                keep_running = false;
            }
            ImGui::EndMenu();
        }
        /*s
        if (ImGui::BeginMenu(fmt::format("{} System", ICON_FA_CALCULATOR).data())) {
            if (!legacy_is_default) {
                if (devices.size() || device_contexts.size()) {
                    for (auto &device : devices)
                        ImGui::TextDisabled(fmt::format("{} {} {} (#{})", ICON_FA_MICROCHIP, device->org, device->name, device->serial).data());
                    if (ImGui::Selectable(fmt::format("{} Release Hardware", ICON_FA_STOP).data())) {
                        // Clear devices and device contexts
                        devices.clear();
                        device_contexts.clear();
                    }
                } else {
                    ImGui::TextDisabled("No devices.");
                }
            }
            if (ImGui::Selectable(fmt::format("{} Clear System Calibrations", ICON_FA_ERASER).data())) {
                // Clear system calibrations (implementation not shown)
            }
            if (!legacy_is_default && ImGui::Selectable(fmt::format("{} {} Legacy Support", ICON_FA_RECYCLE, enable_legacy_support ? "Disable" : "Enable").data())) {
                // Toggle legacy support
                try_toggle_legacy_support();
            }
            ImGui::EndMenu();
        }
        */

        // Get Help button with tooltip
        if (ImGui::SmallButton(fmt::format("{} Get Help", ICON_FA_HANDS_HELPING).data())) {
            // Open the Discord page for help
            ShellExecuteA(0, 0, "https://discord.com/invite/4jNDqjyZnK", 0, 0, SW_SHOW);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("This will take you to our Discord page where you can chat with us.");
            ImGui::EndTooltip();
        }

        ImGui::EndMenuBar();
    }
}
static void emit_legacy_hardware_enablement_error_popup(const glm::ivec2 &framebuffer_size) {
    // Set the position and size of the popup window
    ImGui::SetNextWindowPos({ static_cast<float>((framebuffer_size.x / 2) - 200),
                              static_cast<float>((framebuffer_size.y / 2) - 100) }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ 400, 200 }, ImGuiCond_Always);
    // ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 12, 12 });
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, { 90.f / 255.f, 12.f / 255.f, 12.f / 255.f, 1.f });

    // Begin the popup modal window
    if (ImGui::BeginPopupModal(legacy_is_default ? "Hardware Enablement Error" : "Legacy Hardware Enablement Error",
                               nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        // Display the error message
        ImGui::TextWrapped(fmt::format("{} support was unable to be activated. There are additional drivers required for this functionality. Make sure they're installed.",
                                       legacy_is_default ? "Hardware" : "Legacy hardware").data());
        ImGui::NewLine();

        {
            const auto old_y = ImGui::GetCursorPos().y;
            ImGui::SetCursorPos({ ImGui::GetCursorPos().x, ImGui::GetCursorPos().y + 2 });

            // Display the exclamation triangle icon
            ImGui::Text(fmt::format("{}", ICON_FA_EXCLAMATION_TRIANGLE).data());

            ImGui::SameLine();
            ImGui::SetCursorPos({ ImGui::GetCursorPos().x, old_y });
        }

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        // Display the error description as input text (read-only)
        ImGui::InputText("##LegacySupportErrorDescription", legacy_support_error_description->data(),
                         legacy_support_error_description->size(), ImGuiInputTextFlags_ReadOnly);

        ImGui::SetCursorPos({ ImGui::GetStyle().WindowPadding.x, ImGui::GetWindowSize().y - 30 - ImGui::GetStyle().WindowPadding.y });

        // Display the "Okay" button to close the popup
        ImGui::PushStyleColor(ImGuiCol_Button, { 32.f / 255.f, 32.f / 255.f, 32.f / 255.f, 0.1f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 32.f / 255.f, 32.f / 255.f, 32.f / 255.f, .5f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 32.f / 255.f, 32.f / 255.f, 32.f / 255.f, 1.f });
        if (ImGui::Button("Okay", { ImGui::GetContentRegionAvail().x, 30 })) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::EndPopup();
    }
    // ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void sc::visor::gui::initialize() {
    // Load settings from config file
    if (const auto err = cfg_load(); err) {
        spdlog::error("Unable to load settings: {}", *err);
    } else if (cfg.find("session_token") != cfg.end() && cfg.find("session_person_name") != cfg.end() &&
               cfg.find("session_email") != cfg.end()) {
        // If session token and account information are found in the config, set them
        account_session_token = cfg["session_token"];
        account_person_name = cfg["session_person_name"];
        account_email = cfg["session_email"];
        should_verify_session_token = true;
        spdlog::info("Stored session token: {} ({})", cfg["session_person_name"], cfg["session_token"]);

        // Check the session token with the server
        account_login_response = api::customer::check_session_token(account_email->data(), account_session_token->data());
    }

    // Prepare styling and load animations
    prepare_styling();
    load_animations();

    // Set animation loop options
    animation_scan.loop = true;
    animation_comm.loop = true;

    // Toggle legacy support if it's the default option
    if (legacy_is_default) {
        try_toggle_legacy_support();
    }
}

void sc::visor::gui::shutdown() {
    // Clear device-related data and save settings to config file
    devices_future = { };
    devices.clear();
    animation_scan.frames.clear();
    animation_comm.frames.clear();
    animation_under_construction.frames.clear();
    if (const auto err = cfg_save(); err) {
        spdlog::error("Unable to save settings: {}", *err);
    }
}

void sc::visor::gui::emit(const glm::ivec2 &framebuffer_size, bool *const force_redraw) {
    // Clean up any existing popups
    popups_cleanup();

    // Check if animations were updated and force redraw if necessary
    bool animation_scan_updated = animation_scan.update();
    bool animation_comm_updated = animation_comm.update();
    bool animation_under_construction_updated = animation_under_construction.update();
    if (force_redraw && (animation_scan_updated || animation_comm_updated || animation_under_construction_updated)) {
        *force_redraw = true;
    }

    // Set the position and size of the primary window
    ImGui::SetNextWindowPos({ 0, 0 }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ static_cast<float>(framebuffer_size.x), static_cast<float>(framebuffer_size.y) }, ImGuiCond_Always);

    // Begin the primary window
    if (ImGui::Begin("##PrimaryWindow", nullptr,
                     ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        // Emit the menu bar
        emit_primary_window_menu_bar();

        // Emit the content device panel
        emit_content_device_panel();
    }
    ImGui::End();

    // Emit popups and error messages
    popups_emit(framebuffer_size);
    emit_legacy_hardware_enablement_error_popup(framebuffer_size);

    if (legacy_support_error) {
        // Display legacy support error and open the corresponding popup
        spdlog::error("Legacy support experienced an error.");
        ImGui::OpenPopup(legacy_is_default ? "Hardware Enablement Error" : "Legacy Hardware Enablement Error");
        legacy_support_error = false;
    }

    // Poll devices
    poll_devices();
}
