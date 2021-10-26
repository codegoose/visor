add_library(rest STATIC
    "rest.cxx"
)

target_link_libraries(rest
    CONAN_PKG::libcurl
    CONAN_PKG::nlohmann_json
    CONAN_PKG::tl-expected
    CONAN_PKG::spdlog
)

add_executable(test_rest
    "test_rest.cxx"
)

target_link_libraries(test_rest
    rest
    api
)