#include "api.h"

#include <botan/hex.h>
#include <botan/sha3.h>

#include "../defer.hpp"
#include "../rest/rest.h"

static std::string hash_password(const std::string_view &password) {
    auto hasher = Botan::SHA_3_512();
    hasher.update(password.data());
    return Botan::hex_encode(hasher.final(), false);
}

sc::api::response sc::api::customer::create_new(const std::string_view &email, const std::string_view &name, const std::string_view &password) {
    nlohmann::json doc;
    doc["email"] = email.data();
    doc["name"] = name.data();
    doc["password_hash"] = hash_password(password);
    return std::async(std::launch::async, [doc]() mutable -> tl::expected<nlohmann::json, std::string> {
        return eon::rest::post("http://simcoaches.io/api/customers/create_new", doc);
    });
}

sc::api::response sc::api::customer::get_session_token(const std::string_view &email, const std::string_view &password) {
    nlohmann::json doc;
    doc["email"] = email.data();
    doc["password_hash"] = hash_password(password);
    return std::async(std::launch::async, [doc]() mutable -> tl::expected<nlohmann::json, std::string> {
        return eon::rest::post("http://simcoaches.io/api/customers/get_session_token", doc);
    });
}