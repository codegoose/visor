#pragma once

#include <glm/vec2.hpp>
#include <tl/expected.hpp>

#include <atomic>
#include <mutex>
#include <cstddef>
#include <array>
#include <optional>
#include <string>
#include <memory>
#include <limits>

namespace sc::firmware::mk4 {

    struct device_handle {

        struct axis_info {

            bool enabled = false;
            int8_t curve_i = -1;
            uint16_t min = 0, max = std::numeric_limits<uint16_t>::max();
            uint16_t input = 0, output = 0;
            float input_fraction = 0, output_fraction = 0;
            uint8_t limit = 100;
        };

        std::mutex mutex;
        const uint16_t vendor, product;
        const std::string org, name, uuid, serial;
        void * const ptr;

        uint16_t _communications_id = 0;
        uint16_t _next_packet_id = 0;

        device_handle(const uint16_t &vendor, const uint16_t &product, const std::string_view &org, const std::string_view &name, const std::string_view &uuid, const std::string_view &serial, void * const ptr);
        device_handle(const device_handle&) = delete;
        device_handle &operator=(const device_handle &) = delete;
        ~device_handle();

        std::optional<std::string> write(const std::array<std::byte, 64> &packet);
        tl::expected<std::optional<std::array<std::byte, 64>>, std::string> read(const std::optional<int> &timeout = std::nullopt);
        tl::expected<uint16_t, std::string> get_new_communications_id();
        tl::expected<std::tuple<uint16_t, uint16_t, uint16_t>, std::string> get_version();
        tl::expected<uint8_t, std::string> get_num_axes();
        tl::expected<axis_info, std::string> get_axis_state(const int &index);
        std::optional<std::string> set_axis_enabled(const int &index, const bool &enabled);
        std::optional<std::string> set_axis_range(const int &index, const uint16_t &min, const uint16_t &max, const uint8_t &upper_limit);
        std::optional<std::string> set_axis_bezier_index(const int &index, const int8_t &bezier_index);
        std::optional<std::string> set_bezier_model(const int8_t &index, const std::array<glm::vec2, 6> &model);
        tl::expected<std::array<glm::vec2, 6>, std::string> get_bezier_model(const int8_t &index);
        std::optional<std::string> set_bezier_label(const int8_t &index, const std::string_view &label);
        tl::expected<std::array<char, 50>, std::string> get_bezier_label(const int8_t &index);
        std::optional<std::string> commit();
    };

    tl::expected<std::vector<std::shared_ptr<device_handle>>, std::string> discover(const std::optional<std::vector<std::shared_ptr<device_handle>>> &existing = std::nullopt);
}