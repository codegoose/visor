#pragma once

#include "../firmware/mk4.h"

#include <array>
#include <limits>
#include <mutex>
#include <optional>
#include <future>
#include <vector>
#include <atomic>

#include <glm/vec2.hpp>

namespace sc::visor {

    struct device_context {

        struct axis_info_ex {

            int range_min = 0, range_max = std::numeric_limits<uint16_t>::max();
            int deadzone = 0, limit = 100;

            std::array<glm::ivec2, 6> model = {
                glm::ivec2 { 0, 0 },
                glm::ivec2 { 20, 20 },
                glm::ivec2 { 40, 40 },
                glm::ivec2 { 60, 60 },
                glm::ivec2 { 80, 80 },
                glm::ivec2 { 100, 100 }
            };
        };

        std::mutex mutex;
        std::optional<std::chrono::high_resolution_clock::time_point> last_communication;
        std::shared_ptr<firmware::mk4::device_handle> handle;
        std::atomic_int version_major, version_minor, version_revision;
        std::string name, serial;
        std::vector<firmware::mk4::device_handle::axis_info> axes;
        std::vector<axis_info_ex> axes_ex;
        std::future<std::optional<std::string>> update_future;
        std::atomic_bool initial_communication_complete = false;

        static std::optional<std::string> update(std::shared_ptr<device_context> context);
    };
}