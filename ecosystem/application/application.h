#pragma once

#include "version.h"
#include "joystick.h"

#include <memory>
#include <vector>
#include <optional>
#include <string>
#include <atomic>

#include <glm/vec2.hpp>
#include <SDL2/SDL_joystick.h>

namespace sc::visor {

    extern std::atomic_bool keep_running;

    extern std::vector<std::shared_ptr<joystick>> joysticks;

    void emit_ui(const glm::ivec2 &framebuffer_size);
}