add_library(font STATIC
    "imgui.cxx"
)

target_link_libraries(font
    CONAN_PKG::spdlog
    CONAN_PKG::fmt
    CONAN_PKG::imgui
    CONAN_PKG::freetype

    imgui
    resource
)