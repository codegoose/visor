#pragma once

#include <string_view>

namespace sc::sentry {

    bool initialize(const std::string_view &dsn);
    void shutdown();
}