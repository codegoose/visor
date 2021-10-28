#pragma once

#include <glbinding/gl33core/gl.h>

using namespace gl;

namespace sc::boot::gl {

    void debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *user);
}