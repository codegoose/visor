#include "mk3.h"
#include "firmware.h"

#include "../hidapi/hidapi.h"

#include <spdlog/spdlog.h>
#include <pystring.h>

#include <iostream>

sc::firmware::mk3::device_handle::device_handle(void * const ptr) : ptr(ptr) {

}

sc::firmware::mk3::device_handle::~device_handle() {
    if (ptr) {
        hid_close(reinterpret_cast<hid_device *>(ptr));
        spdlog::debug("Closed MK3 device handle.");
    }
}

tl::expected<std::vector<std::shared_ptr<sc::firmware::mk3::device_handle>>, std::string> sc::firmware::mk3::discover() {
    /*
    std::vector<std::shared_ptr<device_handle>> handles;
    if (const auto devices = hid_enumerate(0x16C0, 0x0476); devices) {
        for (auto info = devices; info = info->next; info) {
            // const auto handle = hid_open_path(info->path);
            const auto handle = hid_open(0x16C0, 0x0476, nullptr);
            if (!handle) {
                spdlog::warn("Unable to open HID: {}/{}", info->vendor_id, info->product_id);
                continue;
            }
            spdlog::debug("Opened HID: {} @ {}", reinterpret_cast<void *>(handle), info->path);
            handles.push_back(std::make_shared<device_handle>(handle));
        }
        hid_free_enumeration(devices);
    }
    return handles;
    */
    std::vector<std::shared_ptr<device_handle>> handles;
    const auto handle = hid_open(0x16C0, 0x0476, nullptr);
    if (!handle) return tl::make_unexpected("Unable to open a handle to the device.");
    handles.push_back(std::make_shared<device_handle>(handle));
    return handles;
}

std::optional<std::string> sc::firmware::mk3::device_handle::write(const std::array<std::byte, 64> &packet) {
    std::vector<std::byte> buffer(packet.size() + 1);
    buffer[0] = static_cast<std::byte>(0x0);
    memcpy(&buffer[1], packet.data(), packet.size());
    if (hid_write(reinterpret_cast<hid_device *>(ptr), reinterpret_cast<const unsigned char *>(buffer.data()), buffer.size()) != buffer.size()) return "Unable to communicate with the device.";
    return std::nullopt;
}

tl::expected<std::optional<std::array<std::byte, 64>>, std::string> sc::firmware::mk3::device_handle::read(const std::optional<int> &timeout) {
    std::array<std::byte, 64> buff_in;
    const auto num_bytes_read = hid_read_timeout(reinterpret_cast<hid_device *>(ptr), reinterpret_cast<unsigned char *>(buff_in.data()), buff_in.size(), timeout ? *timeout : 0);
    if (num_bytes_read == 0) return std::nullopt;
    else if (num_bytes_read == -1) return tl::make_unexpected("There was a problem reading from the device.");
    return buff_in;
}

tl::expected<int, std::string> sc::firmware::mk3::device_handle::version() {
    if (const auto res = write({
        static_cast<std::byte>('S'),
        static_cast<std::byte>('C'),
        static_cast<std::byte>('V')
    }); res) return tl::make_unexpected(*res);
    const auto res = read(2000);
    if (!res.has_value()) return tl::make_unexpected(res.error());
    const auto view = std::string_view(reinterpret_cast<const char *>(res->value().data()), strnlen_s(reinterpret_cast<const char *>(res->value().data()), 64));
    std::vector<std::string> parts;
    pystring::split(static_cast<std::string>(view), parts, ".");
    if (parts.size() == 2 && parts[0] == "MK3" && pystring::isdigit(parts[1])) return std::stoi(parts[1]);
    return tl::make_unexpected("Invalid response.");
}