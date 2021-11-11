#pragma once

#include <glm/vec2.hpp>

#include <string>
#include <string_view>
#include <functional>
#include <optional>

namespace sc::visor::gui {

    struct popup {

        std::string label;
        glm::ivec2 size;
        std::function<void()> emit;

        void launch();
    };

    void initialize();
    void shutdown();
    void emit(const glm::ivec2 &framebuffer_size, bool *const force_redraw = nullptr);
}