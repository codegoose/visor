add_library(firmware STATIC
    "firmware.cxx"
    "mk4.cxx"
)

target_link_libraries(firmware
    CONAN_PKG::spdlog
    CONAN_PKG::fmt
    CONAN_PKG::tl-expected
    CONAN_PKG::pystring
    CONAN_PKG::botan
    CONAN_PKG::glm

    hidapi
)

add_executable(test_firmware_mk4
    "test_firmware_mk4.cxx"
)

target_link_libraries(test_firmware_mk4
    CONAN_PKG::spdlog
    CONAN_PKG::fmt
    CONAN_PKG::tl-expected
    CONAN_PKG::pystring

    firmware
)