#include "discovery.h"

#include "../firmware/firmware.h"
#include "../defer.hpp"

#include <transwarp.h>
#include <spdlog/spdlog.h>

namespace tw = transwarp;

/*

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
        std::vector<std::shared_ptr<firmware::mk4::device_handle>> devices;
        for (;;) {
            DEFER(std::this_thread::sleep_for(std::chrono::seconds(3)));
            if (!keep_running) break;
            if (!worker_mutex.try_lock()) continue;
            DEFER(worker_mutex.unlock());
            const auto res = firmware::mk4::discover(devices);
            if (!res.has_value()) continue;
            for (auto &device : *res) devices.push_back(device);
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

*/

std::shared_future<tl::expected<std::vector<std::shared_ptr<sc::firmware::mk4::device_handle>>, std::string>> sc::visor::discovery::find_mk4(const std::optional<std::vector<std::shared_ptr<firmware::mk4::device_handle>>> &existing) {
    auto task = tw::make_task(tw::root, []{
        return sc::firmware::mk4::discover();
    });
    task->schedule();
    return task->future();
}