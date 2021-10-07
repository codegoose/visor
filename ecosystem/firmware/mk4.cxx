#include "mk4.h"
#include "firmware.h"

#include "../hidapi/hidapi.h"
#include "../defer.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <pystring.h>
#include <botan/auto_rng.h>
#include <glm/common.hpp>

#include <tuple>
#include <algorithm>
#include <iostream>

sc::firmware::mk4::device_handle::device_handle(const uint16_t &vendor, const uint16_t &product, const std::string_view &org, const std::string_view &name, const std::string_view &uuid, const std::string_view &serial, void * const ptr) : vendor(vendor), product(product), org(org), name(name), uuid(uuid), serial(serial), ptr(ptr) {

}

sc::firmware::mk4::device_handle::~device_handle() {
    hid_close(reinterpret_cast<hid_device *>(ptr));
}

tl::expected<std::vector<std::shared_ptr<sc::firmware::mk4::device_handle>>, std::string> sc::firmware::mk4::discover(const std::optional<std::vector<std::shared_ptr<device_handle>>> &existing) {
    firmware::prepare_subsystem();
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
                    } // else spdlog::warn("Unable to validate MK4 HID @ {} ({})", cur_dev->path, comm_res.error());
                    new_device_handle->_communications_id = *comm_res;
                }
            }
        }
        hid_free_enumeration(devs);
    }
    return handles;
}

std::optional<std::string> sc::firmware::mk4::device_handle::write(const std::array<std::byte, 64> &packet) {
    std::lock_guard guard(mutex);
    std::vector<std::byte> buffer(packet.size() + 1);
    buffer[0] = static_cast<std::byte>(0x0);
    memcpy(&buffer[1], packet.data(), packet.size());
    if (hid_write(reinterpret_cast<hid_device *>(ptr), reinterpret_cast<const unsigned char *>(buffer.data()), buffer.size()) != buffer.size()) return "Unable to send data to the device.";
    return std::nullopt;
}

tl::expected<std::optional<std::array<std::byte, 64>>, std::string> sc::firmware::mk4::device_handle::read(const std::optional<int> &timeout) {
    std::lock_guard guard(mutex);
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

tl::expected<std::tuple<uint16_t, uint16_t, uint16_t>, std::string> sc::firmware::mk4::device_handle::get_version() {
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    memcpy(&buffer[2], &_communications_id, sizeof(_communications_id));
    const auto sent_packet_id = _next_packet_id++;
    memcpy(&buffer[4], &sent_packet_id, sizeof(sent_packet_id));
    buffer[6] = static_cast<std::byte>('V');
    if (const auto res = write(buffer); res) return tl::make_unexpected(*res);
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
        if (id != _communications_id || packet_id != sent_packet_id) continue;
        std::tuple<uint16_t, uint16_t, uint16_t> semver;
        memcpy(&std::get<0>(semver), &res.value()->data()[6], sizeof(uint16_t));
        memcpy(&std::get<1>(semver), &res.value()->data()[8], sizeof(uint16_t));
        memcpy(&std::get<2>(semver), &res.value()->data()[10], sizeof(uint16_t));
        return semver;
    }
}

tl::expected<uint8_t, std::string> sc::firmware::mk4::device_handle::get_num_axes() {
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    memcpy(&buffer[2], &_communications_id, sizeof(_communications_id));
    const auto sent_packet_id = _next_packet_id++;
    memcpy(&buffer[4], &sent_packet_id, sizeof(sent_packet_id));
    buffer[6] = static_cast<std::byte>('J');
    buffer[7] = static_cast<std::byte>('A');
    buffer[8] = static_cast<std::byte>('C');
    if (const auto res = write(buffer); res) return tl::make_unexpected(*res);
    const auto start = std::chrono::system_clock::now();
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() > 2000) return tl::make_unexpected("Timed out waiting for axis count from device.");
        const auto res = read(2000);
        if (!res.has_value()) return tl::make_unexpected(res.error());
        if (!res.value().has_value()) continue;
        if (memcmp("SC", res.value()->data(), 2) != 0) continue;
        uint16_t id, packet_id;
        memcpy(&id, &res.value()->data()[2], sizeof(id));
        memcpy(&packet_id, &res.value()->data()[4], sizeof(packet_id));
        if (id != _communications_id || packet_id != sent_packet_id) continue;
        return static_cast<uint8_t>(res.value()->data()[6]);
    }
}

tl::expected<sc::firmware::mk4::device_handle::axis_info, std::string> sc::firmware::mk4::device_handle::get_axis_state(const int &index) {
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    memcpy(&buffer[2], &_communications_id, sizeof(_communications_id));
    const auto sent_packet_id = _next_packet_id++;
    memcpy(&buffer[4], &sent_packet_id, sizeof(sent_packet_id));
    buffer[6] = static_cast<std::byte>('J');
    buffer[7] = static_cast<std::byte>('A');
    buffer[8] = static_cast<std::byte>('S');
    buffer[9] = static_cast<std::byte>(index);
    if (const auto res = write(buffer); res) return tl::make_unexpected(*res);
    const auto start = std::chrono::system_clock::now();
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() > 2000) return tl::make_unexpected("Timed out waiting for axis count from device.");
        const auto res = read(2000);
        if (!res.has_value()) return tl::make_unexpected(res.error());
        if (!res.value().has_value()) continue;
        if (memcmp("SC", res.value()->data(), 2) != 0) continue;
        uint16_t id, packet_id;
        memcpy(&id, &res.value()->data()[2], sizeof(id));
        memcpy(&packet_id, &res.value()->data()[4], sizeof(packet_id));
        if (id != _communications_id || packet_id != sent_packet_id) continue;
        if (res.value()->data()[6] != static_cast<std::byte>(index)) continue;
        axis_info info;
        info.enabled = static_cast<bool>(res.value()->data()[7]);
        info.curve_i = static_cast<int8_t>(res.value()->data()[8]);
        memcpy(&info.min, &res.value()->data()[9], sizeof(info.min));
        memcpy(&info.max, &res.value()->data()[11], sizeof(info.max));
        memcpy(&info.input, &res.value()->data()[13], sizeof(info.input));
        memcpy(&info.output, &res.value()->data()[15], sizeof(info.output));
        memcpy(&info.deadzone, &res.value()->data()[17], sizeof(info.deadzone));
        memcpy(&info.limit, &res.value()->data()[18], sizeof(info.limit));
        info.input_fraction = (double)(info.input - std::numeric_limits<uint16_t>::min()) / (double)(std::numeric_limits<uint16_t>::max() - std::numeric_limits<uint16_t>::min());
        info.output_fraction = (double)(info.output - std::numeric_limits<uint16_t>::min()) / (double)(std::numeric_limits<uint16_t>::max() - std::numeric_limits<uint16_t>::min());
        return info;
    }
}

std::optional<std::string> sc::firmware::mk4::device_handle::set_axis_enabled(const int &index, const bool &enabled) {
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    memcpy(&buffer[2], &_communications_id, sizeof(_communications_id));
    const auto sent_packet_id = _next_packet_id++;
    memcpy(&buffer[4], &sent_packet_id, sizeof(sent_packet_id));
    buffer[6] = static_cast<std::byte>('J');
    buffer[7] = static_cast<std::byte>('A');
    buffer[8] = static_cast<std::byte>('E');
    buffer[9] = static_cast<std::byte>(index);
    buffer[10] = static_cast<std::byte>(enabled);
    if (const auto res = write(buffer); res) return *res;
    const auto start = std::chrono::system_clock::now();
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() > 2000) return "Timed out waiting for axis enablement acknowledgement from device.";
        const auto res = read(2000);
        if (!res.has_value()) return res.error();
        if (!res.value().has_value()) continue;
        if (memcmp("SC", res.value()->data(), 2) != 0) continue;
        uint16_t id, packet_id;
        memcpy(&id, &res.value()->data()[2], sizeof(id));
        memcpy(&packet_id, &res.value()->data()[4], sizeof(packet_id));
        if (id != _communications_id || packet_id != sent_packet_id) continue;
        if (res.value()->data()[6] != static_cast<std::byte>(index)) continue;
        if (res.value()->data()[7] != static_cast<std::byte>(enabled)) continue;
        return std::nullopt;
    }
}

std::optional<std::string> sc::firmware::mk4::device_handle::set_axis_range(const int &index, const uint16_t &min, const uint16_t &max, const uint8_t &deadzone, const uint8_t &upper_limit) {
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    memcpy(&buffer[2], &_communications_id, sizeof(_communications_id));
    const auto sent_packet_id = _next_packet_id++;
    memcpy(&buffer[4], &sent_packet_id, sizeof(sent_packet_id));
    buffer[6] = static_cast<std::byte>('J');
    buffer[7] = static_cast<std::byte>('A');
    buffer[8] = static_cast<std::byte>('R');
    buffer[9] = static_cast<std::byte>(index);
    memcpy(&buffer[10], &min, sizeof(min));
    memcpy(&buffer[12], &max, sizeof(max));
    memcpy(&buffer[14], &deadzone, sizeof(deadzone));
    memcpy(&buffer[15], &upper_limit, sizeof(upper_limit));
    if (const auto res = write(buffer); res) return *res;
    const auto start = std::chrono::system_clock::now();
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() > 2000) return "Timed out waiting for axis range acknowledgement from device.";
        const auto res = read(2000);
        if (!res.has_value()) return res.error();
        if (!res.value().has_value()) continue;
        if (memcmp("SC", res.value()->data(), 2) != 0) continue;
        uint16_t id, packet_id;
        memcpy(&id, &res.value()->data()[2], sizeof(id));
        memcpy(&packet_id, &res.value()->data()[4], sizeof(packet_id));
        if (id != _communications_id || packet_id != sent_packet_id) continue;
        if (res.value()->data()[6] != static_cast<std::byte>(index)) continue;
        if (memcmp(&res.value()->data()[7], &min, sizeof(min)) != 0) continue;
        if (memcmp(&res.value()->data()[9], &max, sizeof(max)) != 0) continue;
        if (res.value()->data()[11] != static_cast<std::byte>(deadzone)) continue;
        if (res.value()->data()[12] != static_cast<std::byte>(upper_limit)) continue;
        return std::nullopt;
    }
}

std::optional<std::string> sc::firmware::mk4::device_handle::set_axis_bezier_index(const int &index, const int8_t &bezier_index) {
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    memcpy(&buffer[2], &_communications_id, sizeof(_communications_id));
    const auto sent_packet_id = _next_packet_id++;
    memcpy(&buffer[4], &sent_packet_id, sizeof(sent_packet_id));
    buffer[6] = static_cast<std::byte>('J');
    buffer[7] = static_cast<std::byte>('A');
    buffer[8] = static_cast<std::byte>('B');
    buffer[9] = static_cast<std::byte>(index);
    buffer[10] = static_cast<std::byte>(bezier_index);
    if (const auto res = write(buffer); res) return *res;
    const auto start = std::chrono::system_clock::now();
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() > 2000) return "Timed out waiting for axis range acknowledgement from device.";
        const auto res = read(2000);
        if (!res.has_value()) return res.error();
        if (!res.value().has_value()) continue;
        if (memcmp("SC", res.value()->data(), 2) != 0) continue;
        uint16_t id, packet_id;
        memcpy(&id, &res.value()->data()[2], sizeof(id));
        memcpy(&packet_id, &res.value()->data()[4], sizeof(packet_id));
        if (id != _communications_id || packet_id != sent_packet_id) continue;
        if (res.value()->data()[6] != static_cast<std::byte>(index)) continue;
        if (res.value()->data()[7] != static_cast<std::byte>(bezier_index)) continue;
        return std::nullopt;
    }
}

std::optional<std::string> sc::firmware::mk4::device_handle::set_bezier_model(const int8_t &index, const std::array<glm::vec2, 6> &model) {
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    memcpy(&buffer[2], &_communications_id, sizeof(_communications_id));
    const auto sent_packet_id = _next_packet_id++;
    memcpy(&buffer[4], &sent_packet_id, sizeof(sent_packet_id));
    buffer[6] = static_cast<std::byte>('B');
    buffer[7] = static_cast<std::byte>('A');
    buffer[8] = static_cast<std::byte>('M');
    buffer[9] = static_cast<std::byte>(index);
    memcpy(&buffer[10], model.data(), sizeof(glm::vec2) * model.size());
    if (const auto res = write(buffer); res) return *res;
    const auto start = std::chrono::system_clock::now();
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() > 2000) return "Timed out waiting for bezier model acknowledgement from device.";
        const auto res = read(2000);
        if (!res.has_value()) return res.error();
        if (!res.value().has_value()) continue;
        if (memcmp("SC", res.value()->data(), 2) != 0) continue;
        uint16_t id, packet_id;
        memcpy(&id, &res.value()->data()[2], sizeof(id));
        memcpy(&packet_id, &res.value()->data()[4], sizeof(packet_id));
        if (id != _communications_id || packet_id != sent_packet_id) continue;
        if (res.value()->data()[6] != static_cast<std::byte>(index)) continue;
        if (memcmp(&res.value()->data()[7], model.data(), sizeof(glm::vec2) * model.size()) != 0) continue;
        return std::nullopt;
    }
}

tl::expected<std::array<glm::vec2, 6>, std::string> sc::firmware::mk4::device_handle::get_bezier_model(const int8_t &index) {
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    memcpy(&buffer[2], &_communications_id, sizeof(_communications_id));
    const auto sent_packet_id = _next_packet_id++;
    memcpy(&buffer[4], &sent_packet_id, sizeof(sent_packet_id));
    buffer[6] = static_cast<std::byte>('B');
    buffer[7] = static_cast<std::byte>('A');
    buffer[8] = static_cast<std::byte>('G');
    buffer[9] = static_cast<std::byte>(index);
    if (const auto res = write(buffer); res) return tl::make_unexpected(*res);
    const auto start = std::chrono::system_clock::now();
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() > 2000) return tl::make_unexpected("Timed out waiting for bezier model acknowledgement from device.");
        const auto res = read(2000);
        if (!res.has_value()) return tl::make_unexpected(res.error());
        if (!res.value().has_value()) continue;
        if (memcmp("SC", res.value()->data(), 2) != 0) continue;
        uint16_t id, packet_id;
        memcpy(&id, &res.value()->data()[2], sizeof(id));
        memcpy(&packet_id, &res.value()->data()[4], sizeof(packet_id));
        if (id != _communications_id || packet_id != sent_packet_id) continue;
        if (res.value()->data()[6] != static_cast<std::byte>(index)) continue;
        std::array<glm::vec2, 6> model;
        memcpy(model.data(), &res.value()->data()[7], sizeof(glm::vec2) * model.size());
        return model;
    }
}

std::optional<std::string> sc::firmware::mk4::device_handle::set_bezier_label(const int8_t &index, const std::string_view &label) {
    if (label.size() > 50) return "Specified label is too long.";
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    memcpy(&buffer[2], &_communications_id, sizeof(_communications_id));
    const auto sent_packet_id = _next_packet_id++;
    memcpy(&buffer[4], &sent_packet_id, sizeof(sent_packet_id));
    buffer[6] = static_cast<std::byte>('B');
    buffer[7] = static_cast<std::byte>('A');
    buffer[8] = static_cast<std::byte>('U');
    buffer[9] = static_cast<std::byte>(index);
    memcpy(&buffer[10], label.data(), glm::min(label.size(), static_cast<size_t>(50)));
    if (const auto res = write(buffer); res) return *res;
    const auto start = std::chrono::system_clock::now();
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() > 2000) return "Timed out waiting for bezier model acknowledgement from device.";
        const auto res = read(2000);
        if (!res.has_value()) return res.error();
        if (!res.value().has_value()) continue;
        if (memcmp("SC", res.value()->data(), 2) != 0) continue;
        uint16_t id, packet_id;
        memcpy(&id, &res.value()->data()[2], sizeof(id));
        memcpy(&packet_id, &res.value()->data()[4], sizeof(packet_id));
        if (id != _communications_id || packet_id != sent_packet_id) continue;
        if (res.value()->data()[6] != static_cast<std::byte>(index)) continue;
        if (memcmp(label.data(), &res.value()->data()[7], label.size()) != 0) continue;
        return std::nullopt;
    }
}

tl::expected<std::array<char, 50>, std::string> sc::firmware::mk4::device_handle::get_bezier_label(const int8_t &index) {
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    memcpy(&buffer[2], &_communications_id, sizeof(_communications_id));
    const auto sent_packet_id = _next_packet_id++;
    memcpy(&buffer[4], &sent_packet_id, sizeof(sent_packet_id));
    buffer[6] = static_cast<std::byte>('B');
    buffer[7] = static_cast<std::byte>('A');
    buffer[8] = static_cast<std::byte>('L');
    buffer[9] = static_cast<std::byte>(index);
    if (const auto res = write(buffer); res) return tl::make_unexpected(*res);
    const auto start = std::chrono::system_clock::now();
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() > 2000) return tl::make_unexpected("Timed out waiting for bezier model acknowledgement from device.");
        const auto res = read(2000);
        if (!res.has_value()) return tl::make_unexpected(res.error());
        if (!res.value().has_value()) continue;
        if (memcmp("SC", res.value()->data(), 2) != 0) continue;
        uint16_t id, packet_id;
        memcpy(&id, &res.value()->data()[2], sizeof(id));
        memcpy(&packet_id, &res.value()->data()[4], sizeof(packet_id));
        if (id != _communications_id || packet_id != sent_packet_id) continue;
        if (res.value()->data()[6] != static_cast<std::byte>(index)) continue;
        std::array<char, 50> reported_label;
        memcpy(reported_label.data(), &res.value()->data()[7], reported_label.size());
        return reported_label;
    }
}

std::optional<std::string> sc::firmware::mk4::device_handle::commit() {
    std::array<std::byte, 64> buffer;
    memset(buffer.data(), 0, buffer.size());
    buffer[0] = static_cast<std::byte>('S');
    buffer[1] = static_cast<std::byte>('C');
    memcpy(&buffer[2], &_communications_id, sizeof(_communications_id));
    const auto sent_packet_id = _next_packet_id++;
    memcpy(&buffer[4], &sent_packet_id, sizeof(sent_packet_id));
    buffer[6] = static_cast<std::byte>('S');
    if (const auto res = write(buffer); res) return *res;
    const auto start = std::chrono::system_clock::now();
    for (;;) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count() > 2000) return "Timed out waiting for commit acknowledgement from device.";
        const auto res = read(2000);
        if (!res.has_value()) return res.error();
        if (!res.value().has_value()) continue;
        if (memcmp("SC", res.value()->data(), 2) != 0) continue;
        uint16_t id, packet_id;
        memcpy(&id, &res.value()->data()[2], sizeof(id));
        memcpy(&packet_id, &res.value()->data()[4], sizeof(packet_id));
        if (id != _communications_id || packet_id != sent_packet_id) continue;
        if (res.value()->data()[6] <= static_cast<std::byte>(0)) return "Chip was unable to write to EEPROM.";
        return std::nullopt;
    }
}