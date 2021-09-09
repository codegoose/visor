#include <thread>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <nlohmann/json.hpp>

#include "../defer.hpp"
#include "serial.h"

const auto comm_port = "\\\\.\\COM7";

/*
0x69
  ? -- Hardware Check
  I -- Informational Text
  C -- Initialize/Clear EEPROM
  Q -- Get EEPROM
*/

tl::expected<std::optional<nlohmann::json>, std::string> get_document(sc::serial::comm_instance &comm, const std::vector<std::byte> &message, bool expect_json = true) {
    if (const auto err = comm.write(message); err) {
        spdlog::error("Unable to send message.");
        return tl::make_unexpected("Unable to write to COMM port.");
    }
    spdlog::info("Message sent. ({} bytes)", message.size());
    std::vector<std::byte> buffer;
    spdlog::info("Receiving...");
    fmt::print("\n");
    while (buffer.size() == 0 || buffer.back() != static_cast<std::byte>(';')) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const auto res = comm.read();
        if (!res.has_value()) {
            spdlog::error(res.error());
            return tl::make_unexpected("Unable to read from COMM port.");
        }
        if (res->size() == 0) continue;
        for (auto &b : *res) fmt::print("{}", static_cast<char>(b));
        buffer.insert(buffer.end(), res->begin(), res->end());
    }
    fmt::print("\n\n");
    spdlog::info("Got {} bytes in total.", buffer.size());
    fmt::print("\n");
    for (auto &b : buffer) fmt::print("{}", static_cast<char>(b));
    fmt::print("\n{}\n\n", spdlog::to_hex(buffer.begin(), buffer.end()));
    if (expect_json) {
        try {
            const auto doc = nlohmann::json::parse(buffer.begin(), buffer.begin() + (buffer.size() - 1));
            fmt::print("{}\n\n", doc.dump(2, ' ', true));
            spdlog::info("Document parsed.");
            return doc;
        } catch (nlohmann::json::exception &exc) {
            spdlog::error("Unable to parse document.");
            return tl::make_unexpected("Unable to parse data as JSON document.");
        }
    } else return std::nullopt;
}

tl::expected<nlohmann::json, std::string> get_eeprom(sc::serial::comm_instance &comm) {
    const auto eeprom = get_document(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('Q') });
    if (!eeprom.has_value()) return tl::make_unexpected(eeprom.error());
    return *eeprom.value();
}

bool clear_eeprom(sc::serial::comm_instance &comm, bool high = true) {
    const auto res = get_document(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('C'), static_cast<std::byte>(high ? 'H' : 'L') }, false);
    if (res.has_value() && !res->has_value()) return true;
    else {
        spdlog::error(res.error());
        return false;
    }
}

bool clear_eeprom_check(sc::serial::comm_instance &comm, bool high = true) {
    if (!clear_eeprom(comm, high)) return false;
    const auto doc = get_eeprom(comm);
    if (!doc.has_value()) {
        spdlog::error(doc.error());
        return false;
    }
    for (auto &value : doc.value()["eeprom"]) if (static_cast<int>(value) != (high ? 255 : 0)) return false;
    return true;
}

bool verify_eeprom(sc::serial::comm_instance &comm) {
    return clear_eeprom_check(comm, true) && clear_eeprom_check(comm, false);
}

int main() {
    spdlog::set_level(spdlog::level::debug);
    sc::serial::comm_instance comm;
    spdlog::debug("{}", reinterpret_cast<void *>(&comm));
    if (const auto err = comm.open(comm_port); err) {
        spdlog::error(*err);
        return 1;
    }
    DEFER(comm.close());
    if (const auto info = get_document(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('I') }); info.has_value()) spdlog::info("Read information from device.");
    else {
        spdlog::error("Unable to read information from device.");
        return 2;
    }
    if (!verify_eeprom(comm)) {
        spdlog::error("Unable to verify EEPROM.");
        return 3;
    }
    spdlog::info("EEPROM is working okay.");
    spdlog::info("Tests completed.");
    return 0;
}