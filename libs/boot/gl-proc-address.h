#pragma once

#include <functional> // Include the library for function objects

#include <glbinding/glbinding.h> // Include the glbinding library for OpenGL bindings

namespace sc::boot::gl {

    extern std::function<glbinding::ProcAddress(const char*)> get_proc_address; // Declare an external function object named "get_proc_address"
}
