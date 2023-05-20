#pragma once

#include <glm/vec2.hpp>  // Include a library for 2D vectors
#include <tl/expected.hpp>  // Include a library for handling expected results

#include <atomic>  // Include a library for atomic operations
#include <mutex>  // Include a library for mutexes (locks)
#include <cstddef>  // Include a library for size-related types
#include <array>  // Include a library for arrays
#include <optional>  // Include a library for optional values
#include <string>  // Include a library for strings
#include <memory>  // Include a library for managing memory
#include <limits>  // Include a library for numeric limits

namespace sc::firmware::mk4 {

    // Define a structure called 'device_handle'
    struct device_handle {

        // Define a structure called 'axis_info'
        struct axis_info {

            bool enabled = false;  // A flag indicating if the axis is enabled
            int8_t curve_i = -1;  // An index for the curve (not important for now)
            uint16_t min = 0, max = std::numeric_limits<uint16_t>::max();  // Minimum and maximum values for the axis
            uint16_t input = 0, output = 0;  // Input and output values for the axis
            float input_fraction = 0, output_fraction = 0;  // Fractional values for input and output
            uint8_t deadzone = 0, limit = 100;  // Deadzone and limit values for the axis
        };

        std::mutex mutex;  // A lock for ensuring thread safety
        const uint16_t vendor, product;  // Vendor and product IDs for the device
        const std::string org, name, uuid, serial;  // Organization, name, UUID, and serial number for the device
        void * const ptr;  // A pointer to the device

        uint16_t _communications_id = 0;  // Internal ID for communication
        uint16_t _next_packet_id = 0;  // Internal ID for the next packet

        // Constructor for the device_handle structure
        device_handle(const uint16_t &vendor, const uint16_t &product, const std::string_view &org, const std::string_view &name, const std::string_view &uuid, const std::string_view &serial, void * const ptr);

        // Disable copy constructor and assignment operator
        device_handle(const device_handle&) = delete;
        device_handle &operator=(const device_handle &) = delete;

        // Destructor for the device_handle structure
        ~device_handle();

        // Function to write a packet of data
        std::optional<std::string> write(const std::array<std::byte, 64> &packet);

        // Function to read a packet of data
        tl::expected<std::optional<std::array<std::byte, 64>>, std::string> read(const std::optional<int> &timeout = std::nullopt);

        // Function to get a new communications ID
        tl::expected<uint16_t, std::string> get_new_communications_id();

        // Function to get the version information
        tl::expected<std::tuple<uint16_t, uint16_t, uint16_t>, std::string> get_version();

        // Function to get the number of axes
        tl::expected<uint8_t, std::string> get_num_axes();

        // Function to get the state of an axis
        tl::expected<axis_info, std::string> get_axis_state(const int &index);

        // Function to set the enabled state of an axis
        std::optional<std::string> set_axis_enabled(const int &index, const bool &enabled);

        // Function to set the range of an axis
        std::optional<std::string> set_axis_range(const int &index, const uint16_t &min, const uint16_t &max, const uint8_t &deadzone, const uint8_t &upper_limit);

        // Function to set the bezier index of an axis
        std::optional<std::string> set_axis_bezier_index(const int &index, const int8_t &bezier_index);

        // Function to set the bezier model
        std::optional<std::string> set_bezier_model(const int8_t &index, const std::array<glm::vec2, 6> &model);

        // Function to get the bezier model
        tl::expected<std::array<glm::vec2, 6>, std::string> get_bezier_model(const int8_t &index);

        // Function to set the bezier label
        std::optional<std::string> set_bezier_label(const int8_t &index, const std::string_view &label);

        // Function to get the bezier label
        tl::expected<std::array<char, 50>, std::string> get_bezier_label(const int8_t &index);

        // Function to commit changes
        std::optional<std::string> commit();
    };

    // Function to discover devices
    tl::expected<std::vector<std::shared_ptr<device_handle>>, std::string> discover(const std::optional<std::vector<std::shared_ptr<device_handle>>> &existing = std::nullopt);
}
