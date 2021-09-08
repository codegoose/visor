#include <thread>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#include "serial.h"

const auto comm_port = "\\\\.\\COM7";

int main() {
    spdlog::set_level(spdlog::level::debug);
    sc::serial::comm_instance comm;
    spdlog::debug("{}", reinterpret_cast<void *>(&comm));
    if (const auto err = comm.open(comm_port); err) {
        spdlog::error(*err);
        return 1;
    }
    std::vector<std::byte> message;
    message.push_back(static_cast<std::byte>(0x69));
    for (int i = 0; i < 3; i++) {
        spdlog::info("Starting iteration #{}.", i + 1);
        if (const auto err = comm.write(message); err) {
            spdlog::error("Unable to send message.");
            return 2;
        }
        spdlog::info("Message sent. ({} bytes)", message.size());
        spdlog::info("Waiting 1 seconds...");
        std::vector<std::byte> buffer;
        while (buffer.size() < 21) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            const auto res = comm.read();
            if (!res.has_value()) {
                spdlog::error(res.error());
                return 3;
            }
            if (res->size() == 0) continue;
            spdlog::debug("Got {} bytes...", res->size());
            buffer.insert(buffer.begin(), res->begin(), res->end());
        }
        spdlog::info("Got {} bytes in total.", buffer.size());
        fmt::print("\n");
        for (auto &b : buffer) fmt::print("{}", static_cast<char>(b));
        fmt::print("\n{}\n\n", spdlog::to_hex(buffer.begin(), buffer.end()));
        if (strncmp("sim_coaches_hardware;", reinterpret_cast<const char *>(buffer.data()), 21) == 0) spdlog::info("Sim Coaches hardware identified.");
        else spdlog::warn("Not Sim Coaches hardware.");
    }
    comm.close();
    spdlog::info("Test complete.");
    return 0;
}