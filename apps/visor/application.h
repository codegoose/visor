#pragma once
// This preprocessor directive ensures that the header file is only included once during compilation to prevent multiple definition errors.

#include "main.rc"
// This includes the resource script file "main.rc" which usually contains resources like icons, bitmaps, cursors, menus, etc. used in the application.

#include <memory>
// This includes the memory library, which provides utilities for managing memory, including smart pointers like std::unique_ptr and std::shared_ptr.

#include <vector>
// The vector library is included to use the std::vector type, a dynamic array.

#include <optional>
// The optional library is included to allow for functions to return a value that could be empty.

#include <string>
// The string library is included to use the std::string type, a sequence of characters.

#include <atomic>
// The atomic library is included to use atomic operations that are critical in multithreaded environments. Atomic operations are performed in a single operation without the possibility of interruption.

#include <glm/vec2.hpp>
// The glm/vec2.hpp library is included to use the glm::dvec2 and glm::ivec2 types, which represent 2-dimensional vectors with double and integer components, respectively.

namespace sc::visor {
// The sc::visor namespace is used to organize the code and avoid name collisions.

    extern std::atomic_bool keep_running;
    // The 'keep_running' atomic boolean is declared as extern, meaning it is defined elsewhere, likely in a .cpp file. Atomic variables are used to guarantee that operations on them are not interrupted or manipulated concurrently by different threads.

    extern bool legacy_support_error;
    // The 'legacy_support_error' boolean is declared as extern, meaning it is defined elsewhere, likely in a .cpp file. This flag might be used to indicate if an error occurred related to legacy support.

    extern std::optional<std::string> legacy_support_error_description;
    // The 'legacy_support_error_description' optional string is declared as extern, meaning it is defined elsewhere, likely in a .cpp file. This optional string might be used to contain a description of an error related to legacy support, if such an error occurred.
}
