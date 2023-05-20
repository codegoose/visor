#pragma once
// This preprocessor directive ensures that the header file is only included once during compilation to prevent multiple definition errors.

#include <vector>
// This includes the vector library, which provides the std::vector type, a dynamic array.

#include <chrono>
// This includes the chrono library, which provides utilities for time measurement.

#include <memory>
// This includes the memory library, which provides utilities for managing memory, including smart pointers like std::unique_ptr and std::shared_ptr.

#include "../../libs/texture/texture.h"
// This includes the texture.h header file from the texture library, likely providing functionality for handling textures in the application.

namespace sc {
// The sc namespace is used to organize the code and avoid name collisions.

    struct animation_instance {
    // This structure defines an animation instance, which includes data needed for managing and updating an animation.

        bool play = false;
        // If set to true, the animation is intended to be playing. This flag could be used to start or stop the animation.

        bool playing = false;
        // If true, the animation is currently playing. This flag could be used to check the state of the animation.

        bool loop = false;
        // If true, the animation should loop back to the beginning after reaching the end.

        double time = 0;
        // This could represent the current time or position in the animation, likely in seconds.

        double frame_rate = 0;
        // This represents the frame rate of the animation, in frames per second.

        size_t frame_i = 0;
        // This represents the index of the current frame in the frames vector.

        std::vector<std::shared_ptr<sc::texture::gpu_handle>> frames;
        // This is a vector of shared pointers to gpu_handle objects in the sc::texture namespace. Each gpu_handle likely represents a frame of the animation, and is stored on the GPU.

        std::chrono::high_resolution_clock::time_point last_update = std::chrono::high_resolution_clock::now();
        // This is the time point of the last update to the animation. It is initialized to the current time when the animation_instance is created.

        bool update();
        // This is likely a function to update the state of the animation based on the elapsed time since the last update. The function could return a boolean indicating whether the update was successful or not.
    };
}
