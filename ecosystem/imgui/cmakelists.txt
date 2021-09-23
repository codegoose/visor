add_library(imgui STATIC
    "imgui_freetype.cpp"
    "imgui_impl_opengl3.cpp"
    "imgui_impl_glfw.cpp"
)

target_link_libraries(imgui
    CONAN_PKG::glfw
    CONAN_PKG::glbinding
    CONAN_PKG::imgui
    CONAN_PKG::freetype
)