add_executable(template
    "main.rc"
    "main.cxx"
)

target_link_libraries(template
    CONAN_PKG::fmt
    CONAN_PKG::glm
    CONAN_PKG::imgui
    CONAN_PKG::argparse
    CONAN_PKG::pystring
    CONAN_PKG::tl-expected

    boot
    imgui
    font
)

win32_release_mode_no_console(template)
