#include "resource.h"

#include <windows.h>

std::optional<std::pair<void *, size_t>> sc::resource::get_resource(const std::string_view &type, const std::string_view &name) {
    auto resource_handle = FindResource(nullptr, name.data(), type.data());
    if (resource_handle == NULL) return std::nullopt;
    auto memory_handle = LoadResource(nullptr, resource_handle);
    if (memory_handle == NULL) return std::nullopt;
    auto resource_length = SizeofResource(nullptr, resource_handle);
    if (resource_length == 0) return std::nullopt;
    auto resource_location = LockResource(memory_handle);
    if (resource_location == 0) return std::nullopt;
    return std::make_pair(resource_location, resource_length);
}