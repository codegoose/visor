#include <functional>

#include <glbinding/glbinding.h>

namespace sc::boot::gl {

    extern std::function<glbinding::ProcAddress(const char*)> get_proc_address;
}