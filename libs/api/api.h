#pragma once

#include <string_view>
#include <future>

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include "../rest/rest.h"

namespace sc::api {

    using response = std::shared_future<eon::rest::response>;

    namespace customer {

        response create_new(const std::string_view &email, const std::string_view &name, const std::string_view &password);
        response check_session_token(const std::string_view &email, const std::string_view &token);
        response get_session_token(const std::string_view &email, const std::string_view &password);
        response activate_account(const std::string_view &email, const std::string_view &code);
        response request_password_reset(const std::string_view &email);
        response password_reset(const std::string_view &email, const std::string_view &code, const std::string_view &password);
    }
}