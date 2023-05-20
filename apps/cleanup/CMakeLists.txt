add_executable(cleanup
    "main.rc"  # Include the resource file "main.rc"
    "main.cxx"  # Include the source file "main.cxx"
)

target_link_libraries(cleanup
    CONAN_PKG::fmt  # Link the "fmt" library from Conan Package Manager
)

win32_release_mode_no_console(cleanup)  # Set the release mode for the Windows platform to not display a console window when running the "cleanup" executable
