add_library(api STATIC
    "api.cxx"
)

target_link_libraries(api
    CONAN_PKG::botan

    rest
)