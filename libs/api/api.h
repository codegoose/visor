#pragma once

#include <string_view> // Include the header file for string views
#include <future> // Include the header file for futures

#include <nlohmann/json.hpp> // Include the header file for JSON parsing
#include <tl/expected.hpp> // Include the header file for expected values

#include "../rest/rest.h" // Include the header file for the REST library

namespace sc::api { // Define a namespace called "sc::api"

    using response = std::shared_future<eon::rest::response>; // Define a type alias "response" as a shared future of REST responses

    namespace customer { // Define a nested namespace "customer" under "sc::api"

        // Declare a function "create_new" that takes email, name, and password as string views and returns a response
        response create_new(const std::string_view &email, const std::string_view &name, const std::string_view &password);

        // Declare a function "check_session_token" that takes email and token as string views and returns a response
        response check_session_token(const std::string_view &email, const std::string_view &token);

        // Declare a function "get_session_token" that takes email and password as string views and returns a response
        response get_session_token(const std::string_view &email, const std::string_view &password);

        // Declare a function "activate_account" that takes email and code as string views and returns a response
        response activate_account(const std::string_view &email, const std::string_view &code);

        // Declare a function "request_password_reset" that takes email as a string view and returns a response
        response request_password_reset(const std::string_view &email);

        // Declare a function "password_reset" that takes email, code, and password as string views and returns a response
        response password_reset(const std::string_view &email, const std::string_view &code, const std::string_view &password);
    }
}
