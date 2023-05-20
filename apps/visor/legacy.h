#pragma once

#include <string> // Include the string header
#include <array> // Include the array header
#include <optional> // Include the optional header
#include <cstdint> // Include the cstdint header
#include <limits> // Include the limits header

#include <glm/vec2.hpp> // Include the glm/vec2 header

namespace sc::visor::legacy {

    // Structure to store information about an axis
    struct axis_info {

        bool present = false; // Flag to indicate if the axis is present
        float input_raw = 0; // Raw input value of the axis
        int input_steps = 0; // Input value in steps
        int output_steps_min = 0, output_steps_max = 1000; // Minimum and maximum output steps
        float output = 0; // Output value of the axis
        int output_short = 0; // Shortened output value of the axis
        int deadzone = 0; // Deadzone value of the axis
        int output_limit = 100; // Output limit value of the axis
        int model_edit_i = -1; // Index of the model being edited
        int curve_i = -1; // Index of the curve assigned to the axis
        std::optional<std::string> label; // Optional label for the axis
        std::array<char, 50> label_buffer; // Buffer for the label
    };

    // Structure to store information about a model
    struct model {

        std::array<glm::ivec2, 6> points = {
            glm::ivec2 { 0, 0 },
            glm::ivec2 { 20, 20 },
            glm::ivec2 { 40, 40 },
            glm::ivec2 { 60, 60 },
            glm::ivec2 { 80, 80 },
            glm::ivec2 { 100, 100 }
        }; // Array of points for the model

        std::optional<std::string> label; // Optional label for the model
        std::array<char, 50> label_buffer = { 0 }; // Buffer for the label
    };

    extern std::array<axis_info, 4> axes; // Array of axis_info structs
    extern std::array<model, 5> models; // Array of model structs

    extern std::optional<int> axis_i_throttle; // Optional index of the throttle axis
    extern std::optional<int> axis_i_brake; // Optional index of the brake axis
    extern std::optional<int> axis_i_clutch; // Optional index of the clutch axis

    std::optional<std::string> enable(); // Function to enable legacy support
    void disable(); // Function to disable legacy support
    std::optional<std::string> process(); // Function to process legacy inputs
    bool present(); // Function to check if legacy hardware is present

    std::optional<std::string> load_settings(); // Function to load legacy settings
    std::optional<std::string> save_settings(); // Function to save legacy settings
}
