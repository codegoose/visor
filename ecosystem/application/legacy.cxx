#include "legacy.h"

#include <optional>

#include <spdlog/spdlog.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include <GLFW/glfw3.h>
#include "../hidhide/hidhide.h"
#include "../vigem/client.h"
#undef min
#undef max

namespace sc::visor::legacy {

    static std::optional<PVIGEM_CLIENT> vigem_client;
    static std::optional<PVIGEM_TARGET> vigem_gamepad;
    static std::optional<XUSB_REPORT> vigem_gamepad_report;

    static std::optional<std::filesystem::path> get_module_file_path() {
        TCHAR path[MAX_PATH];
        if (GetModuleFileNameA(NULL, path, sizeof(path)) == 0) return std::nullopt;
        return path;
    }

    static bool whitelist_this_module() {
        if (auto whitelist = hidhide::get_whitelist(); whitelist.has_value()) {
            if (auto module_path = get_module_file_path(); module_path.has_value()) {
                if (auto image_path = hidhide::convert_path_to_image_path(module_path.value()); image_path.has_value()) {
                    bool whitelisted = false;
                    for (auto &path : *whitelist) {
                        spdlog::debug("HIDHIDE whitelist entry: {}", path.string());
                        if (path == image_path.value()) whitelisted = true;
                    }
                    if (whitelisted) return true;
                    else {
                        whitelist->push_back(*image_path);
                        if (hidhide::set_whitelist(*whitelist)) {
                            spdlog::debug("Added this module to HIDHIDE whitelist: {}", image_path->string());
                            return true;
                        } else spdlog::error("Unable to add this module to HIDHIDE whitelist: {}", image_path->string());
                    }
                } else spdlog::warn("Unable to convert module path to image path.");
            } else spdlog::error("Unable to get module path.");
        } else spdlog::error("Unable to get HIDHIDE whitelist");
        return false;
    }

    static void sync_blacklist() {
        if (auto whitelist = hidhide::get_whitelist(); whitelist.has_value()) {
            if (whitelist->size() > 0) {
                for (auto &item : *whitelist) {
                    spdlog::debug("HIDHIDE whitelist entry: {}", item.string());
                }
            } else spdlog::warn("HIDHIDE whitelist has no entries.");
        } else spdlog::warn("Unable to get HID whitelist.");
        if (auto blacklist = hidhide::get_blacklist(); blacklist.has_value()) {
            for (auto &item : *blacklist) spdlog::debug("HID blacklist entry: {}", item);
            if (auto devices = hidhide::list_devices(); devices.has_value()) {
                std::vector<std::string> to_hide;
                for (auto &device : *devices) {
                    if (device.product_name != "BU0836 Interface") continue;
                    if (std::find(blacklist->begin(), blacklist->end(), device.instance_path) != blacklist->end()) {
                        spdlog::debug("HID entry \"{}\" already exists.", device.instance_path);
                        continue;
                    }
                    spdlog::info("Hiding device: {} -> {}", device.instance_path, device.product_name);
                    to_hide.push_back(device.instance_path);
                }
                if (to_hide.size()) {
                    if (hidhide::set_blacklist(to_hide)) spdlog::info("Updated HID blacklist.");
                    else spdlog::warn("Unable to update HID blacklist.");
                } else spdlog::debug("No HID blacklist update is needed.");
            }
        } else spdlog::warn("Unable to get HID blacklist.");
    }
}

bool sc::visor::legacy::enable() {
    if (!hidhide::present()) {
        spdlog::error("Unable to detect HIDHIDE.");
        return false;
    }
    if (!hidhide::is_enabled() && !hidhide::set_enabled(true)) {
        spdlog::error("Unable to activate HIDHIDE.");
        return false;
    }
    sync_blacklist();
    if (!whitelist_this_module()) return false;
    if (auto new_vigem_client = vigem_alloc(); new_vigem_client) {
        if (VIGEM_SUCCESS(vigem_connect(new_vigem_client))) {
            if (auto new_vigem_gamepad = vigem_target_x360_alloc(); new_vigem_gamepad) {
                if (VIGEM_SUCCESS(vigem_target_add(new_vigem_client, new_vigem_gamepad))) {
                    vigem_client = new_vigem_client;
                    vigem_gamepad = new_vigem_gamepad;
                    spdlog::debug("Legacy support enabled.");
                    return true;
                } else {
                    spdlog::error("Unable to attach gamepad to driver.");
                    vigem_target_free(new_vigem_gamepad);
                    vigem_disconnect(new_vigem_client);
                    vigem_free(new_vigem_client);
                }
            } else {
                vigem_disconnect(new_vigem_client);
                vigem_free(new_vigem_client);
                spdlog::error("Unable to allocate required memory for gamepad target.");
            }
        } else {
            vigem_free(new_vigem_client);
            spdlog::error("Unable to connect to driver");
        }
    } else spdlog::error("Unable to allocate required memory for driver connection.");
    return false;
}

void sc::visor::legacy::disable() {
    if (vigem_gamepad) {
        if (vigem_client) vigem_target_remove(*vigem_client, *vigem_gamepad);
        vigem_target_free(*vigem_gamepad);
        vigem_gamepad.reset();
        spdlog::debug("Disconnected gamepad from driver.");
    }
    if (vigem_client) {
        vigem_disconnect(*vigem_client);
        vigem_free(*vigem_client);
        vigem_client.reset();
        spdlog::debug("Disconnected from driver.");
    }
    spdlog::debug("Legacy support disabled.");
}

void sc::visor::legacy::process() {
    if (!vigem_client || !vigem_gamepad) return;
    if (!vigem_gamepad_report) {
        vigem_gamepad_report = XUSB_REPORT();
        XUSB_REPORT_INIT(&*vigem_gamepad_report);
        spdlog::debug("Initialized gamepad USB report structure.");
    }
    for (int i = 0; i < GLFW_JOYSTICK_LAST; i++) {
        if (!glfwJoystickPresent(i)) continue;
        spdlog::info("#{}: {} ({})", i, glfwGetJoystickName(i), glfwGetJoystickGUID(i));
    }
    if (!VIGEM_SUCCESS(vigem_target_x360_update(*vigem_client, *vigem_gamepad, *vigem_gamepad_report))) {
        spdlog::error("Unable to update virtual gamepad.");
    }
}