#include "api.h" // Include the header file "api.h"

#include <botan/hex.h> // Include the Botan hex library
#include <botan/sha3.h> // Include the Botan SHA-3 library

#include "../defer.hpp" // Include the "defer" utility
#include "../rest/rest.h" // Include the REST library

// Function to hash a password using the SHA-3 algorithm
static std::string hash_password(const std::string_view &password) {
    auto hasher = Botan::SHA_3_512(); // Create an instance of the SHA-3 hash algorithm
    hasher.update(password.data()); // Update the hasher with the password data
    return Botan::hex_encode(hasher.final(), false); // Return the hexadecimal encoded hash of the password
}

// Implementation of the "create_new" function for the "customer" API
sc::api::response sc::api::customer::create_new(const std::string_view &email, const std::string_view &name, const std::string_view &password) {
    nlohmann::json doc; // Create a JSON object to hold the data
    doc["email"] = email.data(); // Set the email field in the JSON object
    doc["name"] = name.data(); // Set the name field in the JSON object
    doc["password_hash"] = hash_password(password); // Set the password_hash field in the JSON object with the hashed password
    return std::async(std::launch::async, [doc]() mutable -> tl::expected<nlohmann::json, std::string> {
        return eon::rest::post("http://simcoaches.io/api/customers/create_new", doc); // Send a POST request with the JSON data to create a new customer
    });
}

// Implementation of the "check_session_token" function for the "customer" API
sc::api::response sc::api::customer::check_session_token(const std::string_view &email, const std::string_view &token) {
    nlohmann::json doc; // Create a JSON object to hold the data
    doc["email"] = email; // Set the email field in the JSON object
    doc["session_token"] = token; // Set the session_token field in the JSON object
    return std::async(std::launch::async, [doc]() mutable -> tl::expected<nlohmann::json, std::string> {
        return eon::rest::post("http://simcoaches.io/api/customers/check_session_token", doc); // Send a POST request with the JSON data to check the session token
    });
}

// Implementation of the "get_session_token" function for the "customer" API
sc::api::response sc::api::customer::get_session_token(const std::string_view &email, const std::string_view &password) {
    nlohmann::json doc; // Create a JSON object to hold the data
    doc["email"] = email.data(); // Set the email field in the JSON object
    doc["password_hash"] = hash_password(password); // Set the password_hash field in the JSON object with the hashed password
    return std::async(std::launch::async, [doc]() mutable -> tl::expected<nlohmann::json, std::string> {
        return eon::rest::post("http://simcoaches.io/api/customers/get_session_token", doc); // Send a POST request with the JSON data to get the session token
    });
}

// Implementation of the "activate_account" function for the "customer" API
sc::api::response sc::api::customer::activate_account(const std::string_view &email, const std::string_view &code) {
    nlohmann::json doc; // Create a JSON object to hold the data
    doc["email"] = email; // Set the email field in the JSON object
    doc["code"] = code; // Set the code field in the JSON object
    return std::async(std::launch::async, [doc]() mutable -> tl::expected<nlohmann::json, std::string> {
        return eon::rest::post("http://simcoaches.io/api/customers/create_new_confirm", doc); // Send a POST request with the JSON data to activate the account
    });
}

// Implementation of the "request_password_reset" function for the "customer" API
sc::api::response sc::api::customer::request_password_reset(const std::string_view &email) {
    nlohmann::json doc; // Create a JSON object to hold the data
    doc["email"] = email; // Set the email field in the JSON object
    return std::async(std::launch::async, [doc]() mutable -> tl::expected<nlohmann::json, std::string> {
        return eon::rest::post("http://simcoaches.io/api/customers/reset_password", doc); // Send a POST request with the JSON data to request a password reset
    });
}

// Implementation of the "password_reset" function for the "customer" API
sc::api::response sc::api::customer::password_reset(const std::string_view &email, const std::string_view &code, const std::string_view &password) {
    nlohmann::json doc; // Create a JSON object to hold the data
    doc["email"] = email; // Set the email field in the JSON object
    doc["code"] = code; // Set the code field in the JSON object
    doc["hash"] = hash_password(password); // Set the hash field in the JSON object with the hashed password
    return std::async(std::launch::async, [doc]() mutable -> tl::expected<nlohmann::json, std::string> {
        return eon::rest::post("http://simcoaches.io/api/customers/reset_password_confirm", doc); // Send a POST request with the JSON data to reset the password
    });
}
