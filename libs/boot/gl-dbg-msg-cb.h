#pragma once  // Ensures this header file is included only once during compilation.

#include <glbinding/gl33core/gl.h>  // Includes the necessary OpenGL header file.

using namespace gl;  // Brings the gl namespace into the current scope for convenience.

namespace sc::boot::gl {  // Starts a new namespace block for sc::boot::gl.

    void debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *user);
    // Declares the debug_message_callback function with the specified parameter types.

}  // End of namespace sc::boot::gl.
