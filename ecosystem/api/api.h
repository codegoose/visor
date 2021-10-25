#pragma once

#include <string_view>
#include <future>

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

namespace sc::api {

    namespace customer {

        std::shared_future<tl::expected<nlohmann::json, std::string>> create_new(const std::string_view &email, const std::string_view &name, const std::string_view &password);
        std::shared_future<tl::expected<nlohmann::json, std::string>> get_session_token(const std::string_view &email, const std::string_view &password);
    }
}