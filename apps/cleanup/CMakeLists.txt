add_executable(cleanup
    "main.rc"
    "main.cxx"
)

win32_require_admin(cleanup)
win32_release_mode_no_console(cleanup)