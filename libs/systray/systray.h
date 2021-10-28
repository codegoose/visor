#pragma once

#include <string_view>
#include <optional>
#include <functional>

namespace sc::systray {

    void enable(std::optional<std::function<void()>> interact_cb);
    void disable();
}