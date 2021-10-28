add_library(iracing STATIC
    "iracing.cxx"
)

target_link_libraries(iracing
    CONAN_PKG::spdlog
    CONAN_PKG::yaml-cpp
    CONAN_PKG::nlohmann_json
)