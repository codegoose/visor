#include <thread>
#include <chrono>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <nlohmann/json.hpp>

#include "../defer.hpp"

#include "serial.h"
#include "mk2/mk2.h"

int main() {
    spdlog::set_level(spdlog::level::debug);
    const auto res = sc::serial::mk2::discover();
    if (!res.has_value()) {
        spdlog::error(res.error());
        return 1;
    }
    spdlog::info("Found {} MK2 generation devices.", res->size());
    for (auto &device : *res) {
        spdlog::info("Discovered {} ({}): firmware version {}, {} axes", device->product_name, device->product_identifier, device->firmware_version, device->num_axes);
        for (int i = 0; i < device->num_axes; i++) {
            const auto axis_res = device->axis_read(i);
            if (!axis_res.has_value()) {
                spdlog::error(axis_res.error());
                continue;
            }
            spdlog::debug(axis_res->dump(2));
        }
    }
    /*
    const auto comm_ports = sc::serial::list_ports();
    if (!comm_ports.has_value()) spdlog::error(comm_ports.error());
    if (!comm_ports->size()) {
        spdlog::error("No COMM ports found.");
        return 4;
    }
    spdlog::info("Found {} COMM ports.", comm_ports->size());
    for (auto &comm_port : *comm_ports) {
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
        {
            const auto eeprom_doc = get_eeprom(comm);
            if (!eeprom_doc.has_value()) {
                spdlog::error("Unable to get EEPROM: {}", eeprom_doc.error());
                return 6;
            }
            spdlog::debug(eeprom_doc->dump());
        }
        if (const auto err = enable_axis(comm, 0); err) {
            spdlog::error("Unable to enable axis: {}", *err);
            return 5;
        }
        {
            const auto eeprom_doc = get_eeprom(comm);
            if (!eeprom_doc.has_value()) {
                spdlog::error("Unable to get EEPROM: {}", eeprom_doc.error());
                return 7;
            }
            spdlog::debug(eeprom_doc->dump());
        }
        spdlog::info("Checking axes...");
        for (auto c : { '0', '1', '2' }) {
            if (const auto axis = get_document(comm, { static_cast<std::byte>(0x69), static_cast<std::byte>('P'), static_cast<std::byte>('A'), static_cast<std::byte>(c) }); axis.has_value()) {
                fmt::print("\n{}\n\n", axis->value().dump(2));
            } else spdlog::error("Unable to poll axis {}: {}", c, axis.error());
        }
    }
    */
    return 0;
}