#pragma once // Include guard to ensure this header is included only once

#include <glm/vec2.hpp> // Include the glm library's vec2.hpp header

#include <string> // Include the string header
#include <string_view> // Include the string_view header
#include <functional> // Include the functional header
#include <optional> // Include the optional header

namespace sc::visor::gui {

    struct popup { // Declare a struct named "popup"

        std::string label; // A string member variable to store the label of the popup
        glm::ivec2 size; // A glm::ivec2 member variable to store the size of the popup
        std::function<void()> emit; // A function member variable to store a callable object that takes no arguments and returns void

        void launch(); // Declare a member function named "launch" that launches the popup
    };

    void initialize(); // Declare a global function named "initialize" that initializes the GUI
    void shutdown(); // Declare a global function named "shutdown" that shuts down the GUI
    void emit(const glm::ivec2 &framebuffer_size, bool *const force_redraw = nullptr);
    // Declare a global function named "emit" that takes the framebuffer size as a glm::ivec2 reference and an optional pointer to a bool, and triggers an event
}
