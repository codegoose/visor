add_executable(launcher WIN32
    "launcher.rc"
    "main.cxx"
)

target_link_libraries(launcher
    CONAN_PKG::spdlog
    CONAN_PKG::fmt
    CONAN_PKG::tl-expected
    CONAN_PKG::pystring

    sentry
)
