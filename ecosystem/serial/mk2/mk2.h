#pragma once

#include <vector>
#include <memory>
#include <string>

#include <tl/expected.hpp>
#include <nlohmann/json_fwd.hpp>

#include "../serial.h"

namespace sc::serial::mk2 {

    tl::expected<std::optional<nlohmann::json>, std::string> communicate(comm_instance &comm, const std::vector<std::byte> &message, bool expect_json = true);

    struct configurator {

        comm_instance comm;

        std::string product_name;
        std::string product_identifier;
        std::string firmware_version;

        int num_axes;

        tl::expected<nlohmann::json, std::string> info_read();
        tl::expected<nlohmann::json, std::string> eeprom_read();
        std::optional<std::string> eeprom_clear(bool high = true);
        std::optional<std::string> eeprom_clear_verify(bool high = true);
        std::optional<std::string> eeprom_verify();
        std::optional<std::string> axis_enable(const int &axis_index);
        std::optional<std::string> axis_disable(const int &axis_index);
        tl::expected<nlohmann::json, std::string> axis_read(const int &axis_index);
    };

    tl::expected<std::vector<std::shared_ptr<configurator>>, std::string> discover();
}