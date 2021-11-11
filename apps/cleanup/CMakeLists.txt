add_executable(cleanup
    "main.rc"
    "main.cxx"
)

target_link_libraries(cleanup
    CONAN_PKG::fmt
)

win32_require_admin(cleanup)
win32_release_mode_no_console(cleanup)