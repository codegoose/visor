#include "device_context.h"

#include "../defer.hpp"

std::optional<std::string> sc::visor::device_context::update(std::shared_ptr<device_context> context) {
    if (!context || !context->handle) return std::nullopt;
    {
        std::lock_guard guard(context->mutex);
        const auto now = std::chrono::high_resolution_clock::now();
        if (!context->last_communication) context->last_communication = now;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *context->last_communication).count() < 2) return std::nullopt;
    }
    DEFER({
        std::lock_guard guard(context->mutex);
        context->last_communication = std::chrono::high_resolution_clock::now();
    });
    if (const auto res = context->handle->get_version(); res.has_value()) {
        context->version_major = std::get<0>(*res);
        context->version_minor = std::get<1>(*res);
        context->version_revision = std::get<2>(*res);
    } else return res.error();
    const auto axes_res = context->handle->get_num_axes();
    if (!axes_res.has_value()) return axes_res.error();
    context->mutex.lock();
    context->axes.resize(*axes_res);
    context->axes_ex.resize(context->axes.size());
    context->mutex.unlock();
    for (int i = 0; i < *axes_res; i++) {
        const auto res = context->handle->get_axis_state(i);
        if (!res.has_value()) return res.error();
        if (!context->initial_communication_complete) {
            context->axes_ex[i].range_min = res->min;
            context->axes_ex[i].range_max = res->max;
        }
        context->axes[i] = *res;
    }
    context->initial_communication_complete = true;
    return std::nullopt;
}