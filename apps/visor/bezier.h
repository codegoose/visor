#pragma once
// This preprocessor directive ensures that the header file is only included once during compilation to prevent multiple definition errors.

#include <optional>
// The optional library is included to allow for functions to return a value that could be empty.

#include <functional>
// The functional library is included to use the std::function type, which allows for passing around callable objects (like functions or lambdas).

#include <vector>
// The vector library is included to use the std::vector type, a dynamic array.

#include <glm/vec2.hpp>
// The glm/vec2.hpp library is included to use the glm::dvec2 and glm::ivec2 types, which represent 2-dimensional vectors with double and integer components, respectively.

namespace sc::bezier {
// The sc::bezier namespace is used to organize the code and avoid name collisions.

    glm::dvec2 calculate(std::vector<glm::dvec2> inputs, double power, std::optional<std::function<void(const std::vector<glm::dvec2> &level)>> callback = std::nullopt);
    // The calculate function is declared. It takes a vector of 2-dimensional double vectors, a power value and an optional callback function. It returns a 2-dimensional double vector.

    namespace ui {
    // The ui namespace is used to further categorize the UI-specific functions.

        void plot_cubic(std::vector<glm::dvec2> inputs, const glm::ivec2 &size, std::optional<double> fraction = std::nullopt, std::optional<double> limit_min = std::nullopt, std::optional<double> limit_max = std::nullopt, std::optional<double> fraction_h = std::nullopt);
        // The plot_cubic function is declared. It takes a vector of 2-dimensional double vectors, a 2-dimensional integer vector representing size, and optional values for fraction, limit_min, limit_max and fraction_h. The function returns void, meaning it performs actions but does not return a value.
    }
}
