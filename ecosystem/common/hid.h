#pragma once

#include <optional>
#include <vector>
#include <string>
#include <filesystem>

namespace sc::hid {
    
    bool present();
    bool is_enabled();
    bool set_enabled(bool enabled);

    std::optional<std::vector<std::string>> get_blacklist();
    std::optional<std::vector<std::filesystem::path>> get_whitelist();

    bool set_blacklist(const std::vector<std::string> &new_list);
    bool set_whitelist(const std::vector<std::filesystem::path> &new_list);

    std::optional<std::filesystem::path> convert_path_to_image_path(std::filesystem::path path);

    struct system_hid {
        std::string instance_path;
        std::string product_name;
    };

    std::optional<std::vector<system_hid>> list_devices();
}