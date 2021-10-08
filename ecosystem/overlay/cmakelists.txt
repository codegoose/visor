add_executable(overlay
    "overlay.rc"
    "main.cxx"
)

target_link_libraries(overlay
    CONAN_PKG::glm
    CONAN_PKG::spdlog
    CONAN_PKG::fmt
    CONAN_PKG::glfw
    CONAN_PKG::glbinding
    CONAN_PKG::imgui
    CONAN_PKG::freetype
    CONAN_PKG::pystring

    boot
    imgui
    hidhide
    font
    vigem
    systray
    sentry
    texture
    file
    cimpl
    firmware
)
