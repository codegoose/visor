#pragma once

#include "version.h"

#include <vector>

#include <glm/vec2.hpp>
#include <SDL2/SDL_joystick.h>

namespace sc::visor {

    extern std::vector<SDL_JoystickID> joysticks;

    void emit_ui(const glm::ivec2 &framebuffer_size);
}