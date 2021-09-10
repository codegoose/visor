#include <thread>
#include <chrono>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <nlohmann/json.hpp>

#include "../defer.hpp"
#include "serial.h"

const auto comm_port = "\\\\.\\COM5";

/*
0x69
  ? -- Hardware Check
  I -- Informational Text
  CL -- Initialize EEPROM, Low
  CL -- Initialize EEPROM, High
  Q -- Get EEPROM
  PA[0/1/2] -- Poll Axis #
*/

tl::expected<std::optional<nlohmann::json>, std::string> get_document(sc::serial::comm_instance &comm, const std::vector<std::byte> &message, bool expect_json = true) {
    if (const auto err = comm.write(message); err) return tl::make_unexpected("Unable to write to COMM port.");
    std::vector<std::byte> buffer;
    auto last_data_time = std::chrono::high_resolution_clock::now();
    while (buffer.size() == 0 || buffer.back() != static_cast<std::byte>(';')) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const auto res = comm.read();
        if (!res.has_value()) return tl::make_unexpected("Unable to read from COMM port.");
        if (res->size() == 0) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - last_data_time).count() > 2000) return tl::make_unexpected("Timed out waiting for data.");
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

tl::expected<nlohmann::json, std::string> get_eeprom(sc::serial::comm_instance &comm) {
    const auto eeprom = get_document(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('Q') });
    if (!eeprom.has_value()) return tl::make_unexpected(eeprom.error());
    return *eeprom.value();
}

std::optional<std::string> clear_eeprom(sc::serial::comm_instance &comm, bool high = true) {
    const auto res = get_document(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('C'), static_cast<std::byte>(high ? 'H' : 'L') }, false);
    if (res.has_value() && !res->has_value()) return std::nullopt;
    else return res.error();
}

std::optional<std::string> clear_eeprom_check(sc::serial::comm_instance &comm, bool high = true) {
    if (const auto err = clear_eeprom(comm, high); err) return err.value();
    const auto doc = get_eeprom(comm);
    if (!doc.has_value()) return doc.error();
    for (auto &value : doc.value()["eeprom"]) if (static_cast<int>(value) != (high ? 255 : 0)) return "Read invalid value from EEPROM.";
    return std::nullopt;
}

std::optional<std::string> verify_eeprom(sc::serial::comm_instance &comm) {
    if (const auto err = clear_eeprom_check(comm, true); err) return err.value();
    if (const auto err = clear_eeprom_check(comm, false); err) return err.value();
    return std::nullopt;
}

int main() {
    spdlog::set_level(spdlog::level::debug);
    sc::serial::comm_instance comm;
    if (const auto err = comm.open(comm_port); err) {
        spdlog::error(*err);
        return 1;
    }
    spdlog::info("Getting device information...");
    if (const auto info = get_document(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('I') }); info.has_value()) fmt::print("\n{}\n\n", info->value().dump(2));
    else {
        spdlog::error("Unable to read information from device: {}", info.error());
        return 2;
    }
    spdlog::info("Testing EEPROM...");
    if (const auto err = verify_eeprom(comm); err) {
        spdlog::error("Unable to verify EEPROM: {}", err.value());
        return 3;
    }
    spdlog::info("Checking axes...");
    for (auto c : { '0', '1', '2' }) {
        if (const auto axis = get_document(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('P'), static_cast<std::byte>('A'), static_cast<std::byte>(c) }); axis.has_value()) {
            fmt::print("\n{}\n\n", axis->value().dump(2));
        } else spdlog::error("Unable to poll axis {}: {}", c, axis.error());
    }
    spdlog::info("Tests completed.");
    return 0;
}