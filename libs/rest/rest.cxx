#include "rest.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include <spdlog/spdlog.h>

#include <mutex>
#include <optional>

#include <sstream>

#include "../defer.hpp"

static size_t curl_write_cb(void *incoming_buffer, size_t element_size, size_t num_elements, void *user) {
    const auto num_bytes = element_size * num_elements;
    auto buffer = reinterpret_cast<std::vector<std::byte> *>(user);
    const auto length_before = buffer->size();
    buffer->resize(buffer->size() + num_bytes);
    memcpy(&(*buffer)[length_before], incoming_buffer, num_bytes);
    return num_bytes;
}

static std::optional<std::string> initialize_winsock() {
    static std::mutex mutex;
    std::lock_guard guard(mutex);
    static WSADATA wsa_data;
    static std::optional<bool> initialized;
    if (initialized) {
        if (*initialized) return std::nullopt;
    } else {
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0) {
            spdlog::debug("Initialized Windows sockets.");
            initialized = true;
            return std::nullopt;
        }
    }
    return "Unable to initialize Windows sockets.";
}

tl::expected<nlohmann::json, std::string> eon::rest::post(const std::string_view &url, const nlohmann::json &post_data) {
    if (const auto err = initialize_winsock(); err) return tl::make_unexpected(*err);
    std::vector<std::byte> buffer;
    auto curl = curl_easy_init();
    if (!curl) return tl::make_unexpected("Unable to initialize.");
    DEFER(curl_easy_cleanup(curl));
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    struct curl_slist *headers = nullptr;
    DEFER(curl_slist_free_all(headers));
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charset: utf-8");
    const auto post_data_str = post_data.dump();
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data_str.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_data_str.size());
    if (auto curl_res = curl_easy_perform(curl); curl_res != CURLE_OK) {
        std::stringstream ss;
        ss << "Unable to initiate request. (Error #";
        ss << curl_res;
        ss << ")";
        return tl::make_unexpected(ss.str());
    }
    if (long code; curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code) != CURLE_OK || code != 200) {
        std::stringstream ss;
        ss << "HTTP error #";
        ss << code;
        ss << ".";
        return tl::make_unexpected(ss.str());
    }
    try {
        return nlohmann::json::parse(std::string_view(reinterpret_cast<char *>(buffer.data()), buffer.size()));
    } catch (const nlohmann::json::exception &exc) {
        return tl::make_unexpected("Unable to parse response.");
    }
}