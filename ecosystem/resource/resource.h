#pragma once

#include <string_view>
#include <optional>
#include <utility>

namespace sc::resource {

    std::optional<std::pair<void *, size_t>> get_resource(const std::string_view &type, const std::string_view &name);
}