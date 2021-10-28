add_library(systray STATIC
    "systray.cxx"
)

target_link_libraries(systray
    CONAN_PKG::spdlog

    shell32
)