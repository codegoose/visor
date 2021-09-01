#pragma once

#include <string_view>

namespace sc::sentry {

    bool initialize(const std::string_view &dsn, const std::string_view &release);
    void shutdown();
}