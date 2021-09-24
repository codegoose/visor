#pragma once

#include <tl/expected.hpp>

#include <array>
#include <optional>
#include <string>
#include <memory>

namespace sc::firmware::mk4 {

    struct device_handle {

        const uint16_t vendor, product;
        const std::string org, name, uuid, serial;
        void * const ptr;

        device_handle(const uint16_t &vendor, const uint16_t &product, const std::string_view &org, const std::string_view &name, const std::string_view &uuid, const std::string_view &serial, void * const ptr);
        device_handle(const device_handle&) = delete;
        device_handle &operator=(const device_handle &) = delete;
        ~device_handle();

        std::optional<std::string> write(const std::array<std::byte, 64> &packet);
        tl::expected<std::optional<std::array<std::byte, 64>>, std::string> read(const std::optional<int> &timeout = std::nullopt);
        tl::expected<int, std::string> version();
    };

    tl::expected<std::vector<std::shared_ptr<device_handle>>, std::string> discover(const std::optional<std::vector<std::shared_ptr<device_handle>>> &existing = std::nullopt);
}