#include "mk4.h"
#include "firmware.h"

#include "../hidapi/hidapi.h"

#include <spdlog/spdlog.h>
#include <pystring.h>

#include <algorithm>
#include <iostream>

sc::firmware::mk4::device_handle::device_handle(const std::string_view &uuid, void * const ptr) : uuid(uuid), ptr(ptr) {

}

sc::firmware::mk4::device_handle::~device_handle() {
    hid_close(reinterpret_cast<hid_device *>(ptr));
    spdlog::debug("Closed MK4 HID @ {} .", uuid);
}

tl::expected<std::vector<std::shared_ptr<sc::firmware::mk4::device_handle>>, std::string> sc::firmware::mk4::discover(const std::optional<std::vector<std::shared_ptr<device_handle>>> &existing) {
    const uint16_t vendor_id = 0x16C0, product_id = 0x0476;
    std::vector<std::shared_ptr<device_handle>> handles;
	if (const auto devs = hid_enumerate(vendor_id, product_id); devs) {
        auto cur_dev = devs;
        while (cur_dev) {
            spdlog::debug("Checking MK4 HID @ {} ...", cur_dev->path);
            if (cur_dev->vendor_id == vendor_id && cur_dev->product_id == product_id) {
                if (existing) {
                    const auto existing_i = std::find_if(existing->begin(), existing->end(), [cur_dev](const std::shared_ptr<device_handle> &existing_handle) {
                        return existing_handle->uuid == cur_dev->path;
                    });
                    if (existing_i != existing->end()) {
                        spdlog::debug("Skipping MK4 HID @ {} (Already open)", cur_dev->path);
                        break;
                    }
                }
                if (const auto handle = hid_open_path(cur_dev->path); handle) {
                    auto new_device_handle = std::make_shared<device_handle>(cur_dev->path, handle);
                    const auto ver_res = new_device_handle->version();
                    if (ver_res.has_value()) {
                        handles.push_back(new_device_handle);
                        spdlog::debug("Opened MK4 HID @ {} (Revision #{})", cur_dev->path, ver_res.value());
                    } else spdlog::warn("Unable to open MK4 HID @ {} (Bad communication)", cur_dev->path);
                }
            }
            cur_dev = cur_dev->next;
        }
        hid_free_enumeration(devs);
    }
    return handles;
}

std::optional<std::string> sc::firmware::mk4::device_handle::write(const std::array<std::byte, 64> &packet) {
    std::vector<std::byte> buffer(packet.size() + 1);
    buffer[0] = static_cast<std::byte>(0x0);
    memcpy(&buffer[1], packet.data(), packet.size());
    if (hid_write(reinterpret_cast<hid_device *>(ptr), reinterpret_cast<const unsigned char *>(buffer.data()), buffer.size()) != buffer.size()) return "Unable to communicate with the device.";
    return std::nullopt;
}

tl::expected<std::optional<std::array<std::byte, 64>>, std::string> sc::firmware::mk4::device_handle::read(const std::optional<int> &timeout) {
    std::array<std::byte, 64> buff_in;
    const auto num_bytes_read = hid_read_timeout(reinterpret_cast<hid_device *>(ptr), reinterpret_cast<unsigned char *>(buff_in.data()), buff_in.size(), timeout ? *timeout : 0);
    if (num_bytes_read == 0) return std::nullopt;
    else if (num_bytes_read == -1) return tl::make_unexpected("There was a problem reading from the device.");
    return buff_in;
}

tl::expected<int, std::string> sc::firmware::mk4::device_handle::version() {
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
    if (parts.size() == 2 && parts[0] == "MK4" && pystring::isdigit(parts[1])) return std::stoi(parts[1]);
    return tl::make_unexpected("Invalid response.");
}