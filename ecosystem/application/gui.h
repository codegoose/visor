#pragma once

#include <glm/vec2.hpp>

namespace sc::visor::gui {

    void initialize();
    void shutdown();
    void emit(const glm::ivec2 &framebuffer_size);
}