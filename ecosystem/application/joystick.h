#pragma once

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <SDL2/SDL_joystick.h>

#undef min
#undef max

namespace sc::visor {

    struct joystick {

        SDL_JoystickID instance_id;

        joystick(const SDL_JoystickID &instance_id);

        bool load();
        bool save();

        struct axis {

            std::optional<std::string> label;

            char label_input_buf[128] = { 0 };

            std::vector<glm::ivec2> model = {
              { 0, 0 },
              { 20, 20 },
              { 40, 40 },
              { 60, 60 },
              { 80, 80 },
              { 100, 100 }
            };

            int16_t val = 0;
            int16_t min = std::numeric_limits<int16_t>::min();
            int16_t max = std::numeric_limits<int16_t>::max();
            
            float fraction = 0;

            int min_buf = std::numeric_limits<int16_t>::min();
            int max_buf = std::numeric_limits<int16_t>::max();
        };

        std::vector<axis> axes;

        struct button {

            std::optional<std::string> label;

            bool state = false;
        };

        std::vector<button> buttons;

        struct hat {

            std::optional<std::string> label;

            uint8_t state = SDL_HAT_CENTERED;
        };

        std::vector<hat> hats;
    };
}