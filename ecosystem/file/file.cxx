#include "file.h"

#include <fstream>

tl::expected<std::vector<std::byte>, std::string> sc::file::load(const std::filesystem::path &path) {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) return tl::make_unexpected(std::strerror(errno));
    const auto end = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    const auto size = end - ifs.tellg();
    if (size == 0) return tl::make_unexpected("Unable to determine file size.");
    std::vector<std::byte> buffer(size);
    if (!ifs.read(reinterpret_cast<char *>(buffer.data()), buffer.size())) return tl::make_unexpected(std::strerror(errno));
    return std::move(buffer);
}