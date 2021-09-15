add_library(texture STATIC
    "texture.cxx"
)

target_link_libraries(texture
    CONAN_PKG::glbinding
    CONAN_PKG::stb
    CONAN_PKG::spdlog
    CONAN_PKG::fmt
    CONAN_PKG::glm
    CONAN_PKG::tl-expected
    CONAN_PKG::nlohmann_json
)