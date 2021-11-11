#pragma once

#include <string>
#include <array>
#include <optional>
#include <cstdint>
#include <limits>

#include <glm/vec2.hpp>

namespace sc::visor::legacy {

    struct axis_info {

        bool present = false;
        float input_raw = 0;
        int input_steps = 0;
        int output_steps_min = 0, output_steps_max = 1000;
        float output = 0;
        int output_short = 0;
        int deadzone = 0;
        int output_limit = 100;
        int model_edit_i = -1;
        int curve_i = -1;
        std::optional<std::string> label;
        std::array<char, 50> label_buffer;
    };

    struct model {

        std::array<glm::ivec2, 6> points = {
            glm::ivec2 { 0, 0 },
            glm::ivec2 { 20, 20 },
            glm::ivec2 { 40, 40 },
            glm::ivec2 { 60, 60 },
            glm::ivec2 { 80, 80 },
            glm::ivec2 { 100, 100 }
        };

        std::optional<std::string> label;
        std::array<char, 50> label_buffer = { 0 };
    };

    extern std::array<axis_info, 4> axes;
    extern std::array<model, 5> models;

    extern std::optional<int> axis_i_throttle;
    extern std::optional<int> axis_i_brake;
    extern std::optional<int> axis_i_clutch;

    std::optional<std::string> enable();
    void disable();
    std::optional<std::string> process();
    bool present();

    std::optional<std::string> load_settings();
    std::optional<std::string> save_settings();
}