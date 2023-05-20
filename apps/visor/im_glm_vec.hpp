#pragma once // Include guard to ensure this header is included only once

#include <imgui.h> // Include the ImGui library's header
#include <glm/vec2.hpp> // Include the glm library's vec2.hpp header

template<typename A, typename B, typename C> static inline B xy_as(const A &in) {
    return B {
        static_cast<C>(in.x),
        static_cast<C>(in.y)
    };
}
// Template function definition named "xy_as" that takes three template parameters: A, B, and C.
// This function converts a vector-like object of type A to type B by casting its coordinates to type C.

#define IM_GLMD2(v) xy_as<ImVec2, glm::dvec2, double>(v)
// Macro definition named "IM_GLMD2" that expands to a call to the "xy_as" function.
// It converts an ImVec2 object to a glm::dvec2 object using double precision.

#define GLMD_IM2(v) xy_as<glm::dvec2, ImVec2, float>(v)
// Macro definition named "GLMD_IM2" that expands to a call to the "xy_as" function.
// It converts a glm::dvec2 object to an ImVec2 object using single precision.
