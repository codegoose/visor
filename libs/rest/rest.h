#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <future>

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

namespace eon::rest {

    using response = tl::expected<nlohmann::json, std::string>;

    response post(const std::string_view &url, const nlohmann::json &post_data);
}