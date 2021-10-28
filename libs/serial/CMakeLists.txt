add_library(serial STATIC
    "serial.cxx"
)

target_link_libraries(serial
    CONAN_PKG::spdlog
    CONAN_PKG::fmt
    CONAN_PKG::tl-expected
    CONAN_PKG::nlohmann_json
)
