#pragma once

#include <vector>
#include <chrono>
#include <memory>

#include "../../libs/texture/texture.h"

namespace sc {

    struct animation_instance {

        bool play = false;
        bool playing = false;
        bool loop = false;
        double time = 0;
        double frame_rate = 0;
        size_t frame_i = 0;
        std::vector<std::shared_ptr<sc::texture::gpu_handle>> frames;
        std::chrono::high_resolution_clock::time_point last_update = std::chrono::high_resolution_clock::now();

        bool update();
    };
}