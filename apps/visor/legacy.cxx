#include "legacy.h" // Include the "legacy.h" header file

#include <optional> // Include the <optional> header
#include <spdlog/spdlog.h> // Include the spdlog library's header

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used components from Windows headers
#include <windows.h> // Include the Windows header
#include <Xinput.h> // Include the Xinput header

#include <GLFW/glfw3.h> // Include the GLFW library's header
#include <nlohmann/json.hpp> // Include the nlohmann/json library's header

#include "../../libs/hidhide/hidhide.h" // Include the hidhide library's header
#include "../../libs/vigem/client.h" // Include the ViGEm library's header

#include <glm/common.hpp> // Include the glm library's common.hpp header

#include "bezier.h" // Include the "bezier.h" header

#include "../../libs/file/file.h" // Include the file library's header

#undef min // Undefine the min macro
#undef max // Undefine the max macro

std::array<sc::visor::legacy::axis_info, 4> sc::visor::legacy::axes; // Define an array of axis_info structs with a size of 4
std::array<sc::visor::legacy::model, 5> sc::visor::legacy::models; // Define an array of model structs with a size of 5

namespace sc::visor::legacy {

    static std::optional<PVIGEM_CLIENT> vigem_client; // Define an optional variable for the ViGEm client
    static std::optional<PVIGEM_TARGET> vigem_gamepad; // Define an optional variable for the ViGEm gamepad
    static std::optional<DS4_REPORT> vigem_gamepad_report; // Define an optional variable for the ViGEm gamepad report

    static bool found_legacy_hardware = false; // Initialize a flag to indicate whether legacy hardware is found

    static std::optional<std::filesystem::path> get_module_file_path() {
        TCHAR path[MAX_PATH];
        if (GetModuleFileNameA(NULL, path, sizeof(path)) == 0) return std::nullopt; // Get the file path of the module
        return path;
    }

    static std::optional<std::string> whitelist_this_module() {
        if (auto whitelist = hidhide::get_whitelist(); whitelist.has_value()) { // Get the HIDHIDE whitelist
            if (auto module_path = get_module_file_path(); module_path.has_value()) { // Get the file path of the module
                if (auto image_path = hidhide::convert_path_to_image_path(module_path.value()); image_path.has_value()) { // Convert the module path to image path
                    bool whitelisted = false;
                    for (auto &path : *whitelist) {
                        spdlog::debug("HIDHIDE whitelist entry: {}", path.string()); // Log the HIDHIDE whitelist entry
                        if (path == image_path.value()) whitelisted = true;
                    }
                    if (whitelisted) return std::nullopt; // If the module is already whitelisted, return
                    else {
                        whitelist->push_back(*image_path); // Add the module to the HIDHIDE whitelist
                        if (hidhide::set_whitelist(*whitelist)) { // Update the HIDHIDE whitelist
                            spdlog::debug("Added this module to HIDHIDE whitelist: {}", image_path->string()); // Log the addition to HIDHIDE whitelist
                            return std::nullopt;
                        } else return fmt::format("Unable to add this module to HIDHIDE whitelist: {}", image_path->string());
                    }
                } else return "Unable to convert module path to image path.";
            } else return "Unable to get path for this module.";
        } else return "Unable to get HIDHIDE whitelist";
    }

    static std::optional<std::string> sync_blacklist() {
        if (auto whitelist = hidhide::get_whitelist(); whitelist.has_value()) { // Get the HIDHIDE whitelist
            if (whitelist->size() > 0) { // Check if the whitelist has entries
                for (auto &item : *whitelist) {
                    spdlog::debug("HIDHIDE whitelist entry: {}", item.string()); // Log the HIDHIDE whitelist entry
                }
            } else spdlog::debug("HIDHIDE whitelist has no entries.");
        } else return "Unable to get HIDHIDE whitelist."; // Unable to get the HIDHIDE whitelist

        if (auto blacklist = hidhide::get_blacklist(); blacklist.has_value()) { // Get the HIDHIDE blacklist
            for (auto &item : *blacklist) spdlog::debug("HID blacklist entry: {}", item); // Log the HID blacklist entry

            if (auto devices = hidhide::list_devices(); devices.has_value()) { // List the HID devices
                std::vector<std::string> to_hide;
                for (auto &device : *devices) {
                    if (device.product_name != "Sim Coaches P1 Pro Pedals") continue; // Check if the device is "Sim Coaches P1 Pro Pedals"
                    if (std::find(blacklist->begin(), blacklist->end(), device.instance_path) != blacklist->end()) {
                        spdlog::debug("HIDHIDE entry \"{}\" already exists.", device.instance_path); // Log if the HIDHIDE entry already exists
                        continue;
                    }
                    spdlog::info("Hiding device: {} -> {}", device.instance_path, device.product_name); // Log the device being hidden
                    to_hide.push_back(device.instance_path); // Add the device to the HIDHIDE blacklist
                }

                if (to_hide.size()) {
                    if (hidhide::set_blacklist(to_hide)) spdlog::info("Updated HID blacklist."); // Update the HIDHIDE blacklist
                    else return "Unable to update HIDHIDE blacklist."; // Unable to update the HIDHIDE blacklist
                } else spdlog::debug("No HIDHIDE blacklist update is needed.");
            }
        } else return "Unable to get HIDHIDE blacklist."; // Unable to get the HIDHIDE blacklist

        return std::nullopt;
    }
}

// Enable the legacy support
std::optional<std::string> sc::visor::legacy::enable() {
    if (!hidhide::present()) return "Unable to detect HIDHIDE driver."; // Check if HIDHIDE driver is present

    if (!hidhide::is_enabled() && !hidhide::set_enabled(true)) return "Unable to activate HIDHIDE."; // Activate HIDHIDE

    if (const auto err = sync_blacklist(); err) return err; // Sync the HIDHIDE blacklist

    if (const auto err = whitelist_this_module(); err) return err; // Whitelist the module

    if (auto new_vigem_client = vigem_alloc(); new_vigem_client) { // Allocate memory for ViGEm client
        if (VIGEM_SUCCESS(vigem_connect(new_vigem_client))) { // Connect to ViGEmBus driver
            if (auto new_vigem_gamepad = vigem_target_ds4_alloc(); new_vigem_gamepad) { // Allocate memory for ViGEm gamepad
                vigem_target_set_pid(new_vigem_gamepad, 0x1209); // Set the product ID of the gamepad
                vigem_target_set_vid(new_vigem_gamepad, 0x0070); // Set the vendor ID of the gamepad

                if (VIGEM_SUCCESS(vigem_target_add(new_vigem_client, new_vigem_gamepad))) { // Add the gamepad to ViGEm client
                    vigem_client = new_vigem_client; // Set the ViGEm client
                    vigem_gamepad = new_vigem_gamepad; // Set the ViGEm gamepad
                    spdlog::debug("Legacy support enabled.");

                    if (const auto err = load_settings(); err) spdlog::error("Unable to load legacy settings: {}", *err); // Load legacy settings
                    else spdlog::debug("Loaded legacy settings");

                    return std::nullopt;
                } else {
                    vigem_target_free(new_vigem_gamepad);
                    vigem_disconnect(new_vigem_client);
                    vigem_free(new_vigem_client);
                    return "Unable to activate ViGEmBus gamepad.";
                }
            } else {
                vigem_disconnect(new_vigem_client);
                vigem_free(new_vigem_client);
                return "Unable to allocate required memory for ViGEmBus gamepad target.";
            }
        } else {
            vigem_free(new_vigem_client);
            return "Unable to connect to ViGEmBus driver.";
        }
    } else return "Unable to allocate required memory for ViGEmBus driver connection.";
}

void sc::visor::legacy::disable() {
    if (vigem_gamepad) {
        if (vigem_client) vigem_target_remove(*vigem_client, *vigem_gamepad); // Remove the gamepad from ViGEm client
        vigem_target_free(*vigem_gamepad); // Free the memory allocated for the gamepad
        vigem_gamepad.reset(); // Reset the gamepad optional
        spdlog::debug("Disconnected gamepad from ViGEmBus driver.");
    }

    if (vigem_client) {
        vigem_disconnect(*vigem_client); // Disconnect from ViGEmBus driver
        vigem_free(*vigem_client); // Free the memory allocated for the client
        vigem_client.reset(); // Reset the client optional
        spdlog::debug("Disconnected from ViGEmBus driver.");
    }

    vigem_gamepad_report.reset(); // Reset the gamepad report optional
    spdlog::debug("Legacy support disabled.");
}

std::optional<std::string> sc::visor::legacy::process() {
    for (auto &axis : axes) {
        axis.present = false;
        axis.input_raw = 0.f;
    }

    if (!vigem_client || !vigem_gamepad) return std::nullopt; // Check if ViGEm client and gamepad are valid

    if (!vigem_gamepad_report) {
        vigem_gamepad_report = DS4_REPORT();
        DS4_REPORT_INIT(&*vigem_gamepad_report);
        vigem_gamepad_report->bThumbLX = 128;
        vigem_gamepad_report->bThumbLY = 128;
        vigem_gamepad_report->bThumbRX = 128;
        vigem_gamepad_report->bThumbRY = 128;
        vigem_gamepad_report->bTriggerL = 128;
        vigem_gamepad_report->bTriggerR = 128;
        spdlog::debug("Initialized gamepad USB report structure.");
    }

    found_legacy_hardware = false;

    for (int i = 0; i < GLFW_JOYSTICK_LAST; i++) {
        if (!glfwJoystickPresent(i)) continue; // Check if the joystick is present

        if (strcmp("Sim Coaches P1 Pro Pedals", glfwGetJoystickName(i)) != 0) continue; // Check if the joystick is "Sim Coaches P1 Pro Pedals"

        int num_axes;
        const auto inputs = glfwGetJoystickAxes(i, &num_axes); // Get the joystick axes

        if (!inputs) continue;

        for (int j = 0; j < glm::min(num_axes, static_cast<int>(axes.size())); j++) {
            axes[j].present = true; // Set the axis as present
            float value = glm::max(0.f, glm::min(1.f, (inputs[j] + 1.f) * .5f)); // Normalize the axis value

            axes[j].input_raw = value; // Set the raw input value

            axes[j].input_steps = glm::round(value * 1000.f); // Convert input value to steps

            auto max_input = axes[j].output_steps_max / 1000.f; // Calculate the maximum input value
            auto min_input = (axes[j].output_steps_min + ((axes[j].output_steps_max - axes[j].output_steps_min) * (axes[j].deadzone / 100.f))) / 1000.f; // Calculate the minimum input value with deadzone

            if (min_input > max_input) min_input = max_input;

            value -= min_input; // Adjust value based on minimum input
            value *= 1.f / (max_input - min_input); // Scale value based on input range
            value = glm::min(1.f, glm::max(0.f, value)); // Clamp value between 0 and 1

            if (axes[j].curve_i >= 0) { // Check if a curve is assigned to the axis
                std::vector<glm::dvec2> model;
                for (auto &percent : legacy::models[axes[j].curve_i].points) {
                    model.push_back({
                        static_cast<double>(percent.x) / 100.0,
                        static_cast<double>(percent.y) / 100.0
                    });
                }
                value = bezier::calculate(model, value).y; // Apply bezier curve transformation to the value
            }

            value = glm::min(value, axes[j].output_limit / 100.f); // Clamp value based on the output limit

            if (j == 0) vigem_gamepad_report->bThumbLX = 127 + glm::round(128 * value); // Set the gamepad report's thumbstick LX value
            else if (j == 1) vigem_gamepad_report->bThumbLY = 127 + glm::round(128 * value); // Set the gamepad report's thumbstick LY value
            else if (j == 2) vigem_gamepad_report->bThumbRX = 127 + glm::round(128 * value); // Set the gamepad report's thumbstick RX value
            else if (j == 3) vigem_gamepad_report->bThumbRY = 127 + glm::round(128 * value); // Set the gamepad report's thumbstick RY value

            axes[j].output = value; // Set the output value for the axis
        }

        found_legacy_hardware = true;
        break;
    }

    vigem_target_ds4_update(*vigem_client, *vigem_gamepad, *vigem_gamepad_report); // Update the ViGEm gamepad with the gamepad report

    return std::nullopt;
}

bool sc::visor::legacy::present() {
    return found_legacy_hardware; // Return whether legacy hardware is present
}

std::optional<std::string> sc::visor::legacy::save_settings() {
    nlohmann::json doc, axes_doc;

    for (int i = 0; i < axes.size(); i++) {
        nlohmann::json axis_doc;
        axis_doc["min"] = axes[i].output_steps_min; // Save the minimum output steps for the axis
        axis_doc["max"] = axes[i].output_steps_max; // Save the maximum output steps for the axis
        axis_doc["deadzone"] = axes[i].deadzone; // Save the deadzone value for the axis
        axis_doc["limit"] = axes[i].output_limit; // Save the output limit value for the axis

        if (axes[i].curve_i != -1) axis_doc["curve"] = axes[i].curve_i; // Save the curve index for the axis

        if (axes[i].label) axis_doc["label"] = axes[i].label->data(); // Save the label for the axis

        axes_doc.push_back(axis_doc);
    }

    doc["axes"] = axes_doc;

    nlohmann::json models_doc;

    for (int i = 0; i < models.size(); i++) {
        nlohmann::json model_doc;

        if (models[i].label) model_doc["label"] = *models[i].label; // Save the label for the model

        nlohmann::json points_doc;

        for (int j = 0; j < models[i].points.size(); j++) {
            nlohmann::json point_doc = {
                { "x", models[i].points[j].x },
                { "y", models[i].points[j].y }
            };
            points_doc.push_back(point_doc); // Save the points of the model
        }

        model_doc["points"] = points_doc;
        models_doc.push_back(model_doc);
    }

    doc["models"] = models_doc;

    auto doc_content = doc.dump(4); // Serialize the JSON document with indentation
    std::vector<std::byte> doc_data;
    doc_data.resize(doc_content.size());
    memcpy(doc_data.data(), doc_content.data(), glm::min(doc_data.size(), doc_content.size()));
    if (const auto err = file::save("virtual-pedals.json", doc_data); err) return *err; // Save the document to a file

    return std::nullopt;
}

std::optional<std::string> sc::visor::legacy::load_settings() {
    const auto load_res = file::load("virtual-pedals.json"); // Load the JSON document from a file

    if (!load_res.has_value()) return load_res.error();

    auto doc = nlohmann::json::parse(*load_res);

    if (auto axes_doc = doc.find("axes"); axes_doc != doc.end() && axes_doc->is_array()) {
        for (int i = 0; i < glm::min(axes_doc->size(), axes.size()); i++) {
            axes[i].output_steps_min = axes_doc->at(i).value("min", 0); // Load the minimum output steps for the axis
            axes[i].output_steps_max = axes_doc->at(i).value("max", 100); // Load the maximum output steps for the axis
            axes[i].deadzone = axes_doc->at(i).value("deadzone", 0); // Load the deadzone value for the axis
            axes[i].output_limit = axes_doc->at(i).value("limit", 100); // Load the output limit value for the axis
            axes[i].curve_i = axes_doc->at(i).value("curve", -1); // Load the curve index for the axis

            axes[i].model_edit_i = axes[i].curve_i;

            if (axes_doc->at(i).find("label") != axes_doc->at(i).end())
                axes[i].label = axes_doc->at(i)["label"]; // Load the label for the axis
        }
    }

    if (auto models_doc = doc.find("models"); models_doc != doc.end() && models_doc->is_array()) {
        for (int i = 0; i < glm::min(models_doc->size(), models.size()); i++) {
            auto model_doc = models_doc->at(i);

            if (auto label = model_doc.find("label"); label != model_doc.end() && label->is_string())
                models[i].label = *label; // Load the label for the model

            if (auto points_doc = model_doc.find("points"); points_doc->is_array()) {
                for (int j = 0; j < points_doc->size(); j++) {
                    models[i].points[j].x = points_doc->at(j).value("x", 0); // Load the X coordinate of the point
                    models[i].points[j].y = points_doc->at(j).value("y", 0); // Load the Y coordinate of the point
                }
            }
        }
    }

    return std::nullopt;
}
