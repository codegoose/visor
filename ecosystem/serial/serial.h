#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <cstddef>

#include <tl/expected.hpp>

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

namespace sc::serial {

    struct comm_instance {

        std::optional<std::string> port;

        bool connected = false;

        HANDLE io_handle = INVALID_HANDLE_VALUE;
        COMSTAT status;
        DWORD error;

        std::optional<std::string> open(const std::string_view &port, std::optional<DWORD> baud_rate = std::nullopt);
        std::optional<std::string> close();
        tl::expected<std::vector<std::byte>, std::string> read();
        std::optional<std::string> write(const std::vector<std::byte> &input);

        ~comm_instance();
    };
}