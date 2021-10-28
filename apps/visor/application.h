#pragma once

#include "main.rc"

#include <memory>
#include <vector>
#include <optional>
#include <string>
#include <atomic>

#include <glm/vec2.hpp>

namespace sc::visor {

    extern std::atomic_bool keep_running;
    extern bool legacy_support_error;
    extern std::optional<std::string> legacy_support_error_description;
}