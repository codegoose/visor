// Declare a static library named "iracing" using the source file "iracing.cxx"
add_library(iracing STATIC    // Define a static library named "iracing"
    "iracing.cxx"             // Specify the source file "iracing.cxx" for the library
)

// Link the "iracing" library with the following Conan packages: spdlog, yaml-cpp, and nlohmann_json
// This ensures that the required dependencies are included when building and using the "iracing" library
target_link_libraries(iracing
    CONAN_PKG::spdlog         // Link the "spdlog" Conan package to the "iracing" library
    CONAN_PKG::yaml-cpp       // Link the "yaml-cpp" Conan package to the "iracing" library
    CONAN_PKG::nlohmann_json  // Link the "nlohmann_json" Conan package to the "iracing" library
)
