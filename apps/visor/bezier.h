#pragma once

#include <optional>
#include <functional>
#include <vector>

#include <glm/vec2.hpp>

namespace sc::bezier {

    glm::dvec2 calculate(std::vector<glm::dvec2> inputs, double power, std::optional<std::function<void(const std::vector<glm::dvec2> &level)>> callback = std::nullopt);

    namespace ui {

        void plot_cubic(std::vector<glm::dvec2> inputs, const glm::ivec2 &size, std::optional<double> fraction = std::nullopt, std::optional<double> limit_min = std::nullopt, std::optional<double> limit_max = std::nullopt, std::optional<double> fraction_h = std::nullopt);
    }
}