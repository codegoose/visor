#include "mk2.h"

/*
0x69
  ? -- Hardware Check
  I -- Informational Text
  CL -- Initialize EEPROM, Low
  CL -- Initialize EEPROM, High
  Q -- Get EEPROM
  PA[0/1/2] -- Poll Axis #
*/

#include <thread>
#include <chrono>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

tl::expected<std::optional<nlohmann::json>, std::string> sc::serial::mk2::communicate(sc::serial::comm_instance &comm, const std::vector<std::byte> &message, bool expect_json) {
    if (const auto err = comm.write(message); err) return tl::make_unexpected("Unable to write to COMM port.");
    std::vector<std::byte> buffer;
    auto last_data_time = std::chrono::high_resolution_clock::now();
    while (buffer.size() == 0 || buffer.back() != static_cast<std::byte>(';')) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const auto res = comm.read();
        if (!res.has_value()) return tl::make_unexpected("Unable to read from COMM port.");
        if (res->size() == 0) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - last_data_time).count() > 2000) {
                /*
                for (auto &c : buffer) fmt::print("{}", static_cast<char>(c));
                fmt::print("\n");
                fmt::print("\n\n{}\n", spdlog::to_hex(buffer));
                */
                return tl::make_unexpected("Timed out waiting for data.");
            }
            else continue;
        }
        last_data_time = std::chrono::high_resolution_clock::now();
        buffer.insert(buffer.end(), res->begin(), res->end());
    }
    if (expect_json) {
        try {
            const auto doc = nlohmann::json::parse(buffer.begin(), buffer.begin() + (buffer.size() - 1));
            return doc;
        } catch (nlohmann::json::exception &exc) {
            return tl::make_unexpected("Unable to parse data as JSON document.");
        }
    } else return std::nullopt;
}

tl::expected<nlohmann::json, std::string> sc::serial::mk2::configurator::info_read() {
    if (const auto info = communicate(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('I') }); info.has_value()) return info->value();
    else return tl::make_unexpected(fmt::format("Unable to read information from device: {}", info.error()));
}

tl::expected<nlohmann::json, std::string> sc::serial::mk2::configurator::eeprom_read() {
    const auto eeprom = communicate(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('Q') });
    if (!eeprom.has_value()) return tl::make_unexpected(eeprom.error());
    return *eeprom.value();
}

std::optional<std::string> sc::serial::mk2::configurator::eeprom_clear(bool high) {
    const auto res = communicate(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('C'), static_cast<std::byte>(high ? 'H' : 'L') }, false);
    if (res.has_value() && !res->has_value()) return std::nullopt;
    else return res.error();
}

std::optional<std::string> sc::serial::mk2::configurator::eeprom_clear_verify(bool high) {
    if (const auto err = eeprom_clear(high); err) return err.value();
    const auto doc = eeprom_read();
    if (!doc.has_value()) return doc.error();
    for (auto &value : doc.value()["eeprom"]) if (static_cast<int>(value) != (high ? 255 : 0)) return "Got invalid byte from EEPROM.";
    return std::nullopt;
}

std::optional<std::string> sc::serial::mk2::configurator::eeprom_verify() {
    for (const auto flag : { true, false }) if (const auto err = eeprom_clear_verify(flag); err) return err.value();
    return std::nullopt;
}

std::optional<std::string> sc::serial::mk2::configurator::axis_enable(const int &axis_index) {
    std::optional<char> c;
    switch (axis_index) {
        case 0: c = '0'; break;
        case 1: c = '1'; break;
        case 2: c = '2'; break;
        default: return "Invalid axis index.";
    }
    if (const auto res = communicate(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('P'), static_cast<std::byte>('A'), static_cast<std::byte>('E'), static_cast<std::byte>(*c) }, false); !res.has_value()) return res.error();
    else return std::nullopt;
}

std::optional<std::string> sc::serial::mk2::configurator::axis_disable(const int &axis_index) {
    std::optional<char> c;
    switch (axis_index) {
        case 0: c = '0'; break;
        case 1: c = '1'; break;
        case 2: c = '2'; break;
        default: return "Invalid axis index.";
    }
    if (const auto res = communicate(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('P'), static_cast<std::byte>('A'), static_cast<std::byte>('D'), static_cast<std::byte>(*c) }, false); !res.has_value()) return res.error();
    else return std::nullopt;
}

tl::expected<nlohmann::json, std::string> sc::serial::mk2::configurator::axis_read(const int &axis_index) {
    std::optional<char> c;
    switch (axis_index) {
        case 0: c = '0'; break;
        case 1: c = '1'; break;
        case 2: c = '2'; break;
        default: return tl::make_unexpected("Invalid axis index.");
    }
    if (const auto axis = communicate(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('P'), static_cast<std::byte>('A'), static_cast<std::byte>(*c) }); axis.has_value()) return axis->value();    
    else return tl::make_unexpected(fmt::format("Unable to poll axis {}: {}", *c, axis.error()));
}

tl::expected<std::vector<std::shared_ptr<sc::serial::mk2::configurator>>, std::string> sc::serial::mk2::discover() {
    const auto comm_ports = sc::serial::list_ports();
    if (!comm_ports.has_value()) return tl::make_unexpected(comm_ports.error());
    if (!comm_ports->size()) return tl::make_unexpected("No COMM ports found.");
    std::vector<std::shared_ptr<configurator>> collection;
    for (auto &comm_port : *comm_ports) {
        auto instance = std::make_shared<configurator>();
        if (const auto err = instance->comm.open(comm_port); err) continue;
        const auto info_res = instance->info_read();
        if (!info_res.has_value()) continue;
        if (info_res->find("communication_language") == info_res->end()) continue;
        if ((*info_res)["communication_language"] != "mk2") continue;
        if (info_res->find("product_identifier") == info_res->end()) continue;
        const std::string product_identifier = (*info_res)["product_identifier"];
        if (product_identifier != "p1x-smart-pedals") continue;
        instance->product_identifier = product_identifier;
        if (info_res->find("product_name") == info_res->end()) continue;
        const std::string product_name = (*info_res)["product_name"];
        instance->product_name = product_name;
        if (info_res->find("firmware_version") == info_res->end()) continue;
        const std::string firmware_version = (*info_res)["firmware_version"];
        instance->firmware_version = firmware_version;
        if (info_res->find("inputs") == info_res->end()) continue;
        if (!(*info_res)["inputs"].is_array()) continue;
        instance->num_axes = (*info_res)["inputs"].size();
        collection.push_back(instance);
    }
    return collection;
}