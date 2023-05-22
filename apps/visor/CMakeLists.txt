add_executable(visor
    "main.rc" # Include the main resource file
    "main.cxx" # Source file for the main program
    "application.cxx" # Source file for the application logic
    "gui.cxx" # Source file for the GUI implementation
    "gui-iRacing.cxx" # Source file for the iRacing GUI implementation
    "version.cxx" # Source file for handling version information
    "animation_instance.cxx" # Source file for handling animation instances
    "device_context.cxx" # Source file for managing device context
    "legacy.cxx" # Source file for legacy support
    "bezier.cxx" # Source file for bezier curve calculations
)

target_link_libraries(visor
    CONAN_PKG::glm #Link the glm package
    CONAN_PKG::spdlog #Link the spdlog package
    CONAN_PKG::fmt #Link the fmt package
    CONAN_PKG::glfw #Link the glfw package
    CONAN_PKG::glbinding #Link the glbinding package
    CONAN_PKG::imgui #Link the imgui package
    CONAN_PKG::freetype #Link the freetype package
    CONAN_PKG::pystring #Link the pystring package
    CONAN_PKG::magic_enum #Link the magic_enum package
    CONAN_PKG::argparse #Link the argparse package

    api #Link the api library
    boot #Link the boot library
    imgui #Link the imgui library
    hidhide #Link the hidhide library
    font #Link the font library
    vigem #Link the vigem library
    systray #Link the systray library
    sentry #Link the sentry library
    texture #Link the texture library
    file #Link the file library
    cimpl #Link the cimpl library
    firmware #Link the firmware library
    iracing #Link the iracing library
)

win32_release_mode_no_console(visor) #Set the Win32 release mode to not display a console window for the "visor" executable
