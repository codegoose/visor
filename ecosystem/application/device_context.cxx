#include "device_context.h"

#include "../defer.hpp"

#include <glm/common.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

std::optional<std::string> sc::visor::device_context::update(std::shared_ptr<device_context> context) {
    if (!context || !context->handle) return std::nullopt;
    std::lock_guard guard(context->mutex);
    {
        const auto now = std::chrono::high_resolution_clock::now();
        if (!context->last_communication) context->last_communication = now;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *context->last_communication).count() < 10) return std::nullopt;
    }
    DEFER(context->last_communication = std::chrono::high_resolution_clock::now(););
    if (const auto res = context->handle->get_version(); res.has_value()) {
        context->version_major = std::get<0>(*res);
        context->version_minor = std::get<1>(*res);
        context->version_revision = std::get<2>(*res);
    } else return res.error();
    const auto axes_res = context->handle->get_num_axes();
    if (!axes_res.has_value()) return axes_res.error();
    context->axes.resize(*axes_res);
    context->axes_ex.resize(context->axes.size());
    for (int axis_i = 0; axis_i < *axes_res; axis_i++) {
        const auto res = context->handle->get_axis_state(axis_i);
        if (!res.has_value()) return res.error();
        if (!context->initial_communication_complete) {
            context->axes_ex[axis_i].range_min = res->min;
            context->axes_ex[axis_i].range_max = res->max;
            context->axes_ex[axis_i].limit = res->limit;
            context->axes_ex[axis_i].model_edit_i = res->curve_i;
        }
        context->axes[axis_i] = *res;
    }
    if (!context->initial_communication_complete) {
        for (int model_i = 0; model_i < context->models.size(); model_i++) {
            const auto label_res = context->handle->get_bezier_label(model_i);
            if (!label_res.has_value()) return label_res.error();
            if (strnlen_s(label_res->data(), 50) > 0) {
                context->models[model_i].label = label_res->data();
                memcpy(context->models[model_i].label_buffer.data(), label_res->data(), label_res->size());
            }
            const auto model_res = context->handle->get_bezier_model(model_i);
            if (!model_res.has_value()) return model_res.error();
            for (int element_i = 0; element_i < context->models[model_i].points.size(); element_i++) {
                context->models[model_i].points[element_i].x = glm::round((model_res.value()[element_i].x * static_cast<float>(std::numeric_limits<uint16_t>::max())) / 655.35f);
                context->models[model_i].points[element_i].y = glm::round((model_res.value()[element_i].y * static_cast<float>(std::numeric_limits<uint16_t>::max())) / 655.35f);
                spdlog::debug("Curve Info: model #{}, point #{}: {}, {}", model_i, element_i, context->models[model_i].points[element_i].x, context->models[model_i].points[element_i].y);
            }
        }
    }
    context->initial_communication_complete = true;
    return std::nullopt;
}