#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#include "../hidapi/hidapi.h"
#include "../defer.hpp"

#include "mk4.h"

#include <array>

namespace sl = spdlog;

int main() {
    sl::default_logger()->set_level(sl::level::debug);
    const auto handle = hid_open(0x2341, 0x8036, nullptr);
    if (!handle) {
        sl::error("Unable to open device.");
        return 1;
    }
    DEFER(hid_close(handle));
    sl::info("Opened: {}", reinterpret_cast<void *>(handle));
    {
        std::array<std::byte, 64> packet;
        memset(packet.data(), 0, packet.size());
        strcpy(reinterpret_cast<char *>(packet.data()), "Hello, World!");
        std::vector<std::byte> buffer(packet.size() + 1);
        buffer[0] = static_cast<std::byte>(0x1);
        memcpy(&buffer[1], packet.data(), packet.size());
        const auto num_bytes_sent = hid_write(handle, reinterpret_cast<const unsigned char *>(buffer.data()), buffer.size());
        sl::info("Bytes sent: {}", num_bytes_sent);
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    {
        std::array<std::byte, 64> buff_in;
        memset(buff_in.data(), 0, buff_in.size());
        const auto num_bytes_read = hid_read_timeout(handle, reinterpret_cast<unsigned char *>(buff_in.data()), buff_in.size(), 200);
        sl::info("Bytes read: {}", num_bytes_read);
        if (num_bytes_read) {
            sl::warn(sl::to_hex(buff_in));
            sl::critical(reinterpret_cast<char *>(buff_in.data()));
        }
    }
    return 0;
}