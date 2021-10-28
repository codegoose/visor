#pragma once

#include <imgui.h>
#include <glm/vec2.hpp>

template<typename A, typename B, typename C> static inline B xy_as(const A &in) {
    return B {
        static_cast<C>(in.x),
        static_cast<C>(in.y)
    };
}

#define IM_GLMD2(v) xy_as<ImVec2, glm::dvec2, double>(v)
#define GLMD_IM2(v) xy_as<glm::dvec2, ImVec2, float>(v) 