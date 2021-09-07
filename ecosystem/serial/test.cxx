#include <thread>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#include "serial.h"

const auto comm_port = "\\\\.\\COM5";
const auto comm_baud_rate = 9600;

int main() {
    spdlog::set_level(spdlog::level::debug);
    sc::serial::comm_instance comm;
    spdlog::debug("{}", reinterpret_cast<void *>(&comm));
    if (const auto err = comm.open(comm_port, comm_baud_rate); err) {
        spdlog::error(*err);
        return 1;
    }
    for (int i = 0; i < 3; i++) {
        spdlog::info("Starting iteration #{}.", i + 1);
        spdlog::info("Waiting 4 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(4));
        const auto res = comm.read();
        if (!res.has_value()) {
            spdlog::error(res.error());
            return 2;
        }
        spdlog::info("Got {} bytes.", res->size());
        fmt::print("\n");
        for (auto &b : *res) fmt::print("{}", static_cast<char>(b));
        fmt::print("\n{}\n\n", spdlog::to_hex(res->begin(), res->end()));
    }
    comm.close();
    spdlog::info("Test complete.");
    return 0;
}