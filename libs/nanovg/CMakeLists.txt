add_library(nanovg STATIC
    "nanovg.c"
)

target_compile_definitions(nanovg PUBLIC
    NANOVG_GL3_IMPLEMENTATION
)

target_link_libraries(nanovg
    CONAN_PKG::stb
)