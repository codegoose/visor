add_executable(template
    "main.rc" // Include the main resource file
    "main.cxx" // Source file for the main program
)

target_link_libraries(template
    CONAN_PKG::fmt // Link the fmt package
    CONAN_PKG::glm // Link the glm package
    CONAN_PKG::imgui // Link the imgui package
    CONAN_PKG::argparse // Link the argparse package
    CONAN_PKG::pystring // Link the pystring package
    CONAN_PKG::tl-expected // Link the tl-expected package

    boot // Link the boot library
    imgui // Link the imgui library
    font // Link the font library
)

win32_release_mode_no_console(template) // Set the Win32 release mode to not display a console window for the "template" executable

