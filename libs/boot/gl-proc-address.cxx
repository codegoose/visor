#include <spdlog/spdlog.h> // Include the library for logging
#include <glbinding/gl33core/gl.h> // Include the OpenGL binding library
#include <GLFW/glfw3.h> // Include the GLFW library for windowing

#include "gl-proc-address.h" // Include the header file for getting the OpenGL function address

using namespace gl; // Use the gl namespace from the glbinding library

// Define a function named "get_proc_address" with a specific signature and implementation
std::function<glbinding::ProcAddress(const char*)> sc::boot::gl::get_proc_address = [](const char *name) -> glbinding::ProcAddress {
    // Get the address of the OpenGL function using GLFW's getProcAddress
    const auto addr = glfwGetProcAddress(name);
    // Create a text with the function name and its address
    const auto text = fmt::format("GL: {} @ {}", name, reinterpret_cast<void *>(addr));
    if (addr) {
        // If the address is valid, log a debug message with the text
        spdlog::debug(text);
    } else {
        // If the address is invalid, log a warning message with the text, flush the log, and exit with code 1000
        spdlog::warn(text);
        spdlog::default_logger()->flush();
        exit(1000);
    }
    // Return the address as a glbinding ProcAddress
    return reinterpret_cast<glbinding::ProcAddress>(addr);
};
