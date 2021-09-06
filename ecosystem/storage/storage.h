#pragma once

#include <string>
#include <string_view>
#include <optional>

#include <tl/expected.hpp>

namespace sc::storage {

    std::optional<std::string> initialize();
    std::optional<std::string> shutdown();
    std::optional<std::string> sync();

    void set_flag(const std::string_view &key, const bool &value);
    tl::expected<bool, std::string> get_flag(const std::string_view &key, const std::optional<bool> &default_value = std::nullopt);
}