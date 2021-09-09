#include <thread>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#include "../defer.hpp"
#include "serial.h"

const auto comm_port = "\\\\.\\COM7";

/*
0x69
  ? -- Hardware Check
  I -- Informational Text
*/

std::optional<int> check_hardware_id(sc::serial::comm_instance &comm) {
    std::vector<std::byte> message;
    message.push_back(static_cast<std::byte>(0x69));
    message.push_back(static_cast<std::byte>('?'));
    if (const auto err = comm.write(message); err) {
        spdlog::error("Unable to send message.");
        return 2;
    }
    spdlog::info("Message sent. ({} bytes)", message.size());
    spdlog::info("Waiting 1 seconds...");
    std::vector<std::byte> buffer;
    while (buffer.size() == 0 || buffer.back() != static_cast<std::byte>(';')) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const auto res = comm.read();
        if (!res.has_value()) {
            spdlog::error(res.error());
            return 3;
        }
        if (res->size() == 0) continue;
        buffer.insert(buffer.begin(), res->begin(), res->end());
    }
    spdlog::info("Got {} bytes in total.", buffer.size());
    fmt::print("\n");
    for (auto &b : buffer) fmt::print("{}", static_cast<char>(b));
    fmt::print("\n{}\n\n", spdlog::to_hex(buffer.begin(), buffer.end()));
    if (strncmp("sim_coaches_hardware;", reinterpret_cast<const char *>(buffer.data()), 21) != 0) {
        spdlog::warn("Not Sim Coaches hardware.");
        return 4;
    }
    spdlog::info("Sim Coaches hardware identified.");
    return std::nullopt;
}

std::optional<int> get_information_text(sc::serial::comm_instance &comm) {
    std::vector<std::byte> message;
    message.push_back(static_cast<std::byte>(0x69));
    message.push_back(static_cast<std::byte>('I'));
    if (const auto err = comm.write(message); err) {
        spdlog::error("Unable to send message.");
        return 5;
    }
    spdlog::info("Message sent. ({} bytes)", message.size());
    spdlog::info("Waiting 1 seconds...");
    std::vector<std::byte> buffer;
    while (buffer.size() == 0 || buffer.back() != static_cast<std::byte>(';')) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const auto res = comm.read();
        if (!res.has_value()) {
            spdlog::error(res.error());
            return 6;
        }
        if (res->size() == 0) continue;
        buffer.insert(buffer.begin(), res->begin(), res->end());
    }
    spdlog::info("Got {} bytes in total.", buffer.size());
    fmt::print("\n");
    for (auto &b : buffer) fmt::print("{}", static_cast<char>(b));
    fmt::print("\n{}\n\n", spdlog::to_hex(buffer.begin(), buffer.end()));
    return std::nullopt;
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
    if (const auto err = check_hardware_id(comm); err) {
        spdlog::error("Test #1 failed with error #{}.", *err);
        return *err;
    }
    if (const auto err = get_information_text(comm); err) {
        spdlog::error("Test #2 failed with error #{}.", *err);
        return *err;
    }
    spdlog::info("Tests completed.");
    return 0;
}