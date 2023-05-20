#include "device_context.h"
// This includes the device_context.h header file, which contains the definition of the device_context structure.

#include "../../libs/defer.hpp"
// This includes the defer.h header file, which provides the DEFER macro that allows you to schedule code to be executed when the current scope is exited.

#include <glm/common.hpp>
// This includes the glm/common.hpp header file, which provides common GLM functions.

#include <spdlog/spdlog.h>
// This includes the spdlog/spdlog.h header file, which provides the spdlog library for logging.

#include <spdlog/fmt/bin_to_hex.h>
// This includes the spdlog/fmt/bin_to_hex.h header file, which provides spdlog's binary to hexadecimal formatting functions.

std::optional<std::string> sc::visor::device_context::update(std::shared_ptr<device_context> context) {
    // This function updates the state of a device_context. It returns an optional string error message.

    if (!context || !context->handle) return std::nullopt;
    // If the device_context or its handle is null, return no error.

    std::lock_guard guard(context->mutex);
    // Lock the device_context's mutex to prevent other threads from modifying it during this update.

    {
        const auto now = std::chrono::high_resolution_clock::now();
        // Get the current time.

        if (!context->last_communication) context->last_communication = now;
        // If the device_context has not yet communicated, set its last communication time to the current time.

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *context->last_communication).count() < 10) return std::nullopt;
        // If less than 10 milliseconds have passed since the last communication, return no error.
    }

    DEFER(context->last_communication = std::chrono::high_resolution_clock::now(););
    // Schedule the device_context's last communication time to be updated to the current time when the current scope is exited.

    if (const auto res = context->handle->get_version(); res.has_value()) {
        // If the device's version number is available...

        context->version_major = std::get<0>(*res);
        context->version_minor = std::get<1>(*res);
        context->version_revision = std::get<2>(*res);
        // ...update the device_context's version number.

    } else return res.error();
    // If the device's version number is not available, return the error message.

    const auto axes_res = context->handle->get_num_axes();
    if (!axes_res.has_value()) return axes_res.error();
    // If the number of axes of the device is not available, return the error message.

    context->axes.resize(*axes_res);
    context->axes_ex.resize(context->axes.size());
    // Resize the device_context's axes and axes_ex vectors to match the number of axes of the device.

    for (int axis_i = 0; axis_i < *axes_res; axis_i++) {
        // For each axis of the device...

        const auto res = context->handle->get_axis_state(axis_i);
        // ...get the state of the axis...

        if (!res.has_value()) return res.error();
        // ...and if the state is not available, return the error message.

        if (!context->initial_communication_complete) {
            // If the device_context has not yet completed its initial communication...

            context->axes_ex[axis_i].range_min = res->min;
            context->axes_ex[axis_i].range_max = res->max;
            context->axes_ex```c++
[axis_i].deadzone = res->deadzone;
            context->axes_ex[axis_i].limit = res->limit;
            context->axes_ex[axis_i].model_edit_i = res->curve_i;
            // ...update the axis_ex's extended attributes.
        }

        context->axes[axis_i] = *res;
        // Update the axis's state.
    }

    if (!context->initial_communication_complete) {
        // If the device_context has not yet completed its initial communication...

        for (int model_i = 0; model_i < context->models.size(); model_i++) {
            // For each model of the device...

            const auto label_res = context->handle->get_bezier_label(model_i);
            // ...get the label of the model...

            if (!label_res.has_value()) return label_res.error();
            // ...and if the label is not available, return the error message.

            if (strnlen_s(label_res->data(), 50) > 0) {
                // If the label is not empty...

                context->models[model_i].label = label_res->data();
                // ...update the model's label...

                memcpy(context->models[model_i].label_buffer.data(), label_res->data(), label_res->size());
                // ...and copy the label to the model's label buffer.
            }

            const auto model_res = context->handle->get_bezier_model(model_i);
            // Get the bezier model of the model...

            if (!model_res.has_value()) return model_res.error();
            // ...and if the bezier model is not available, return the error message.

            for (int element_i = 0; element_i < context->models[model_i].points.size(); element_i++) {
                // For each point of the bezier model...

                context->models[model_i].points[element_i].x = glm::round((model_res.value()[element_i].x * static_cast<float>(std::numeric_limits<uint16_t>::max())) / 655.35f);
                context->models[model_i].points[element_i].y = glm::round((model_res.value()[element_i].y * static_cast<float>(std::numeric_limits<uint16_t>::max())) / 655.35f);
                // ...update the point's coordinates...

                spdlog::debug("Curve Info: model #{}, point #{}: {}, {}", model_i, element_i, context->models[model_i].points[element_i].x, context->models[model_i].points[element_i].y);
                // ...and log the point's coordinates.
            }
        }
    }

    context->initial_communication_complete = true;
    // Set the device_context's initial communication complete flag to true.

    return std::nullopt;
    // Return no error.
}
