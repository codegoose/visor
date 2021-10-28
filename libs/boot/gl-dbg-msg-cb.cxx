#include "gl-dbg-msg-cb.h"

#include <sentry.h>
#include <spdlog/spdlog.h>

void sc::boot::gl::debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *user) {
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        spdlog::error("GL Error: {}", message);
        spdlog::default_logger()->flush();
        const auto sentry_event = sentry_value_new_message_event(SENTRY_LEVEL_ERROR, "OpenGL", message);
        sentry_event_value_add_stacktrace(sentry_event, nullptr, 0);
        sentry_capture_event(sentry_event);
    } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) spdlog::warn("GL Error: {}", message);
    else if (severity == GL_DEBUG_SEVERITY_LOW) spdlog::debug("GL Error: {}", message);
    else {
        if (strstr(message, "GL_STREAM_DRAW") || strstr(message, "GL_STATIC_DRAW")) return; // yeh i know pls stop
        spdlog::debug("GL Notification: {}", message);
    }
}