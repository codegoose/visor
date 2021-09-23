#pragma once

#include "version.h"

#include <memory>
#include <vector>
#include <optional>
#include <string>
#include <atomic>

#include <glm/vec2.hpp>

namespace sc::visor {

    extern std::atomic_bool keep_running;

    void emit_ui(const glm::ivec2 &framebuffer_size);
}