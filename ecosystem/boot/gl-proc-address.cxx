#include <spdlog/spdlog.h>
#include <glbinding/gl33core/gl.h>
#include <SDL2/SDL.h>

#include "gl-proc-address.h"

using namespace gl;

std::function<glbinding::ProcAddress(const char*)> sc::boot::gl::get_proc_address = [](const char *name) -> glbinding::ProcAddress {
    const auto addr = SDL_GL_GetProcAddress(name);
    const auto text = fmt::format("GL: {} @ {}", name, reinterpret_cast<void *>(addr));
    if (addr) spdlog::debug(text);
    else {
        spdlog::warn(text);
        exit(1000);
    }
    return reinterpret_cast<glbinding::ProcAddress>(addr);
};