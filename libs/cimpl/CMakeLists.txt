add_library(cimpl STATIC
    "cimpl.cxx"
)

target_link_libraries(cimpl
    CONAN_PKG::stb
)