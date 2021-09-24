#pragma once

#include <future>

#include "../firmware/mk4.h"

namespace sc::visor::discovery {

    std::shared_future<tl::expected<std::vector<std::shared_ptr<firmware::mk4::device_handle>>, std::string>> find_mk4(const std::optional<std::vector<std::shared_ptr<firmware::mk4::device_handle>>> &existing = std::nullopt);
}