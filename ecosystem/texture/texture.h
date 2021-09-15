#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include <tl/expected.hpp>
#include <glm/vec2.hpp>

namespace sc::texture {

    struct frame {

        glm::ivec2 size;
        std::vector<std::byte> content;
    };

    struct frame_sequence {

        double frame_rate, duration;
        std::vector<frame> frames;
    };

    struct gpu_handle {

        gpu_handle(const uint32_t &handle, const glm::ivec2 &size);
        gpu_handle(const gpu_handle &) = delete;
        gpu_handle &operator=(const gpu_handle &) = delete;

        ~gpu_handle();

        const uint32_t handle;
        const glm::ivec2 size;
    };

    tl::expected<frame, std::string> load_from_memory(void *data_address, const size_t &data_length);
    tl::expected<frame_sequence, std::string> load_lottie_from_memory(const std::string_view &cache_key, void *data_address, const size_t &data_length, const glm::ivec2 &size);
    tl::expected<frame, std::string> resize(const frame &reference, const glm::ivec2 &new_size);
    tl::expected<std::shared_ptr<gpu_handle>, std::string> upload_to_gpu(const frame &reference, const glm::ivec2 &size);
}