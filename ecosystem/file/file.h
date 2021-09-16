#pragma once

#include <cstddef>
#include <vector>
#include <string>
#include <filesystem>

#include <tl/expected.hpp>

namespace sc::file {

    tl::expected<std::vector<std::byte>, std::string> load(const std::filesystem::path &path); 
}