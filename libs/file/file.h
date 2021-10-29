#pragma once

#include <cstddef>
#include <vector>
#include <string>
#include <filesystem>
#include <optional>

#include <tl/expected.hpp>

namespace sc::file {

    tl::expected<std::vector<std::byte>, std::string> load(const std::filesystem::path &path);
    std::optional<std::string> save(const std::filesystem::path &path, const std::vector<std::byte> &data);
}