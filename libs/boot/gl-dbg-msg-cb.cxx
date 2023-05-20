// Define a callback function named "debug_message_callback" which is used as the debug message callback for OpenGL
void sc::boot::gl::debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *user) {
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        // If the severity of the debug message is high, log an error message using the "spdlog" library
        spdlog::error("GL Error: {}", message);
        // Flush the log to ensure the error message is written immediately
        spdlog::default_logger()->flush();
        // Create a Sentry event with the error message and capture it
        const auto sentry_event = sentry_value_new_message_event(SENTRY_LEVEL_ERROR, "OpenGL", message);
        sentry_event_value_add_stacktrace(sentry_event, nullptr, 0);
        sentry_capture_event(sentry_event);
    } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
        // If the severity of the debug message is medium, log a warning message
        spdlog::warn("GL Error: {}", message);
    } else if (severity == GL_DEBUG_SEVERITY_LOW) {
        // If the severity of the debug message is low, log a debug message
        spdlog::debug("GL Error: {}", message);
    } else {
        // If the severity of the debug message is not high, medium, or low,
        // check if the message contains "GL_STREAM_DRAW" or "GL_STATIC_DRAW".
        // If so, return and do nothing.
        if (strstr(message, "GL_STREAM_DRAW") || strstr(message, "GL_STATIC_DRAW"))
            return; // Skip processing for known irrelevant messages

        // Log a debug message for other notifications
        spdlog::debug("GL Notification: {}", message);
    }
}
