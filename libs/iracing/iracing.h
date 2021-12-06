#pragma once

#include <map>
#include <atomic>
#include <string>

namespace sc::iracing {

    enum class status {

        stopped,
        searching,
        connected,
        live
    };

    void startup();
    void shutdown();

    const status &get_status();

    std::map<std::string, int> variables();

    const std::atomic<bool> &prev();
    const std::atomic<float> &lap_percent();
    const std::atomic<float> &rpm();
    const std::atomic<float> &rpm_prev();
    const std::atomic<float> &speed();
    const std::atomic<float> &speed_prev();
    const std::atomic<int> &gear();
    const std::atomic<int> &gear_prev();
}
