#include "mk4.h"
#include "firmware.h"

#include "../hidapi/hidapi.h"
#include "../defer.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <pystring.h>
#include <botan/auto_rng.h>

#include <tuple>
#include <algorithm>
#include <iostream>

sc::firmware::mk4::device_handle::device_handle(const uint16_t &vendor, const uint16_t &product, const std::string_view &org, const std::string_view &name, const std::string_view &uuid, const std::string_view &serial, void * const ptr) : vendor(vendor), product(product), org(org), name(name), uuid(uuid), serial(serial), ptr(ptr) {

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
            DEFER(cur_dev = cur_dev->next);
            // spdlog::debug("Checking MK4 HID @ {} ...", cur_dev->path);
            if (cur_dev->vendor_id == vendor_id && cur_dev->product_id == product_id) {
                if (existing) {
                    const auto existing_i = std::find_if(existing->begin(), existing->end(), [cur_dev](const std::shared_ptr<device_handle> &existing_handle) {
                        return existing_handle->uuid == cur_dev->path;
                    });
                    if (existing_i != existing->end()) {
                        // spdlog::debug("Skipping MK4 HID @ {} (Already open)", cur_dev->path);
                        break;
                    }
                }
                if (const auto handle = hid_open_path(cur_dev->path); handle) {
                    std::vector<char> serial_buffer(256);
                    size_t num_serial_bytes;
                    if (wcstombs_s(&num_serial_bytes, serial_buffer.data(), serial_buffer.size(), cur_dev->serial_number, serial_buffer.size()) != 0) continue;
                    std::vector<char> org_buffer(256);
                    size_t num_org_bytes;
                    if (wcstombs_s(&num_org_bytes, org_buffer.data(), org_buffer.size(), cur_dev->manufacturer_string, org_buffer.size()) != 0) continue;
                    std::vector<char> name_buffer(256);
                    size_t num_name_bytes;
                    if (wcstombs_s(&num_name_bytes, name_buffer.data(), name_buffer.size(), cur_dev->product_string, name_buffer.size()) != 0) continue;
                    auto new_device_handle = std::make_shared<device_handle>(cur_dev->vendor_id, cur_dev->product_id, org_buffer.data(), name_buffer.data(), cur_dev->path, serial_buffer.data(), handle);
                    const auto comm_res = new_device_handle->get_new_communications_id();
                    if (comm_res.has_value()) {
                        handles.push_back(new_device_handle);
                        spdlog::debug("Opened MK4 HID @ {} (Communications ID: {})", cur_dev->path, comm_res.value());
                    } else spdlog::warn("Unable to validate MK4 HID @ {} ({})", cur_dev->path, comm_res.error());
                    new_device_handle->_communications_id = *comm_res;
                }
            }
        }
        hid_free_enumeration(devs);
    }
    return handles;
}

std::optional<std::string> sc::firmware::mk4::device_handle::write(const std::array<std::byte, 64> &packet) {
    std::vector<std::byte> buffer(packet.size() + 1);
    buffer[0] = static_cast<std::byte>(0x0);
    memcpy(&buffer[1], packet.data(), packet.size());
    if (hid_write(reinterpret_cast<hid_device *>(ptr), reinterpret_cast<const unsigned char *>(buffer.data()), buffer.size()) != buffer.size()) return "Unable to send data to the device.";
    return std::nullopt;
}

tl::expected<std::optional<std::array<std::byte, 64>>, std::string> sc::firmware::mk4::device_handle::read(const std::optional<int> &timeout) {
    std::array<std::byte, 64> buff_in;
    const auto num_bytes_read = hid_read_timeout(reinterpret_cast<hid_device *>(ptr), reinterpret_cast<unsigned char *>(buff_in.data()), buff_in.size(), timeout ? *timeout : 0);
    if (num_bytes_read == 0) return std::nullopt;
    else if (num_bytes_read == -1) return tl::make_unexpected("Unable to read data from the device.");
    return buff_in;
}

tl::expected<uint16_t, std::string> sc::firmware::mk4::device_handle::get_new_communications_id() {
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    buffer[2] = static_cast<std::byte>('!');
    auto rng = std::make_unique<Botan::AutoSeeded_RNG>();
    rng->randomize(reinterpret_cast<uint8_t *>(&buffer[3]), 55);
    spdlog::debug(spdlog::to_hex(buffer));
    if (const auto res = write(buffer); res) return tl::make_unexpected(*res);
    const auto start = std::chrono::system_clock::now();
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() > 2000) return tl::make_unexpected("Timed out waiting for communications ID from device.");
        const auto res = read(2000);
        if (!res.has_value()) return tl::make_unexpected(res.error());
        if (!res.value().has_value()) continue;
        if (memcmp("SC#", res.value()->data(), 3) != 0) continue;
        if (memcmp(&buffer.data()[3], &res.value()->data()[5], 55) != 0) continue;
        uint16_t id;
        memcpy(&id, &res.value()->data()[3], sizeof(uint16_t));
        return id;
    }
}

tl::expected<std::tuple<uint16_t, uint16_t, uint16_t>, std::string> sc::firmware::mk4::device_handle::version() {
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    memcpy(&buffer[2], &_communications_id, sizeof(_communications_id));
    memcpy(&buffer[4], &_next_packet_id, sizeof(_next_packet_id));
    buffer[6] = static_cast<std::byte>('V');
    if (const auto res = write(buffer); res) return tl::make_unexpected(*res);
    const auto sent_packet_id = _next_packet_id++;
    const auto start = std::chrono::system_clock::now();
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() > 2000) return tl::make_unexpected("Timed out waiting for communications ID from device.");
        const auto res = read(2000);
        if (!res.has_value()) return tl::make_unexpected(res.error());
        if (!res.value().has_value()) continue;
        if (memcmp("SC", res.value()->data(), 2) != 0) continue;
        uint16_t id, packet_id;
        memcpy(&id, &res.value()->data()[2], sizeof(id));
        memcpy(&packet_id, &res.value()->data()[4], sizeof(packet_id));
        if (id != _communications_id || packet_id != sent_packet_id) {
            spdlog::error("{}, {}", _communications_id, packet_id);
            continue;
        }
        std::tuple<uint16_t, uint16_t, uint16_t> semver;
        memcpy(&std::get<0>(semver), &res.value()->data()[6], sizeof(uint16_t));
        memcpy(&std::get<1>(semver), &res.value()->data()[8], sizeof(uint16_t));
        memcpy(&std::get<2>(semver), &res.value()->data()[10], sizeof(uint16_t));
        return semver;
    }
}