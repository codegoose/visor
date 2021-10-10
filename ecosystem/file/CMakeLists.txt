add_library(file STATIC
    "file.cxx"
)

target_link_libraries(file
    CONAN_PKG::tl-expected
)