#pragma once

// This is a common C++ directive that helps ensure that a header file is included only once by the preprocessor in a single compilation.

#include "../../libs/firmware/mk4.h"
// This includes the mk4.h header file, which probably contains the definition of the firmware::mk4::device_handle structure.

#include <array>
// This includes the array header file, which provides the std::array template class that encapsulates fixed-size arrays.

#include <limits>
// This includes the limits header file, which provides the std::numeric_limits template that gives information about numeric limits.

#include <mutex>
// This includes the mutex header file, which provides the std::mutex class that represents a mutex.

#include <optional>
// This includes the optional header file, which provides the std::optional template class that represents optional objects.

#include <future>
// This includes the future header file, which provides the std::future class template that represents a provider of a value that may be available at some point in the future.

#include <vector>
// This includes the vector header file, which provides the std::vector template class that represents dynamic arrays.

#include <atomic>
// This includes the atomic header file, which provides the std::atomic template class that represents objects that provide fine-grained atomic operations.

#include <glm/vec2.hpp>
// This includes the glm/vec2.hpp header file, which provides the glm::ivec2 class that represents 2-dimensional vectors.

namespace sc::visor {

    // The sc::visor namespace is being defined.

    struct device_context {
        // The device_context structure is being defined.

        struct axis_info_ex {
            // The axis_info_ex structure is being defined.

            int range_min = 0, range_max = std::numeric_limits<uint16_t>::max();
            int deadzone = 0, limit = 100;
            int model_edit_i = -1;
            // These are the members of the axis_info_ex structure, with their default values.
        };

        struct model {
            // The model structure is being defined.

            std::array<glm::ivec2, 6> points = {
                glm::ivec2 { 0, 0 },
                glm::ivec2 { 20, 20 },
                glm::ivec2 { 40, 40 },
                glm::ivec2 { 60, 60 },
                glm::ivec2 { 80, 80 },
                glm::ivec2 { 100, 100 }
            };
            // This is an array of 6 2-dimensional vectors representing the points of the model, with their default values.

            std::optional<std::string> label;
            // This is an optional string representing the label of the model.

            std::array<char, 50> label_buffer = { 0 };
            // This is an array of 50 characters representing the label buffer of the model, with its default value.
        };

        std::array<model, 5> models;
        // This is an array of 5 models.

        std::mutex mutex;
        // This is a mutex used to synchronize access to the device_context.

        std::optional<std::chrono::high_resolution_clock::time_point> last_communication;
        // This is an optional time point representing the last time the device_context communicated.

        std::shared_ptr<firmware::mk4::device_handle> handle;
        // This is a shared pointer to a device handle.

        std::atomic_int version_major, version_minor, version_revision;
        // These are atomic integers representing the major, minor, and revision versions of the device_context.

        std::string name, serial;
        // These are strings representing the name and serial```c++
        // number of the device_context.

        std::vector<firmware::mk4::device_handle::axis_info> axes;
        // This is a vector of axis information for the device, as defined in the firmware::mk4::device_handle.

        std::vector<axis_info_ex> axes_ex;
        // This is a vector of extended axis information for the device, as defined in axis_info_ex structure.

        std::future<std::optional<std::string>> update_future;
        // This is a future that holds the result of an asynchronous operation that returns an optional string. 
        // This could be used to track the progress of an update operation on the device context.

        std::atomic_bool initial_communication_complete = false;
        // This is an atomic boolean that indicates whether the initial communication with the device has been completed.

        static std::optional<std::string> update(std::shared_ptr<device_context> context);
        // This is a declaration of a static function named 'update' that takes a shared pointer to a device context and returns an optional string.
        // The optional string return value suggests that the function may return a string (possibly an error message or status information) or no value.
        // Since this function is static, it can be called without an instance of device_context.
    };
}
