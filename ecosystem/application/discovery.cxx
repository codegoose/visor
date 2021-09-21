#include "discovery.h"

#include "../firmware/firmware.h"
#include "../defer.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace sc::visor::discovery {

    std::atomic_bool keep_running = false;
    std::thread worker;
    std::mutex worker_mutex;

    void routine() {
        firmware::prepare_subsystem();
        spdlog::debug("Worker has started: Discovery");
        for (;;) {
            DEFER(std::this_thread::sleep_for(std::chrono::milliseconds(10)));
            if (!keep_running) break;
            if (!worker_mutex.try_lock()) continue;
            DEFER(worker_mutex.unlock());
            const auto res = firmware::mk3::discover();
            if (!res.has_value()) continue;
            for (auto &device : *res) {
                const auto version = device->version();
                if (!version.has_value()) {
                    spdlog::error(version.error());
                    continue;
                }
                spdlog::debug("HID: R{}", *version);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        spdlog::debug("Worker has stopped: Discovery");
    }
}

void sc::visor::discovery::startup() {
    if (keep_running) return;
    keep_running = true;
    worker = std::thread(routine);
}

void sc::visor::discovery::shutdown() {
    if (!keep_running) return;
    keep_running = false;
    if (worker.joinable()) worker.join();
}