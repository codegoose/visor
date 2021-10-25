#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <future>

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

namespace eon::rest {

    tl::expected<nlohmann::json, std::string> post(const std::string_view &url, const nlohmann::json &post_data);
}