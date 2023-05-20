# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

# Set the project name
project(simcoaches)

# Include necessary CMake scripts
# Include Conan build info
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)

# Include utility functions
include(${PROJECT_SOURCE_DIR}/scripts/cmake/utility.cmake)

# Include C++ standards enforcement
include(${PROJECT_SOURCE_DIR}/scripts/cmake/cxx-standards.cmake)

# Include precompiled files configuration
include(${PROJECT_SOURCE_DIR}/precompiled/precompiled.cmake)

# Disable default manifest on Win32 platform
win32_disable_default_manifest()

# Enforce C++17 compiler standard
cxx_enforce_17()

# Basic setup for Conan targets
conan_basic_setup(TARGETS)

# Add subdirectories for libraries and applications
# Add libraries subdirectory
add_subdirectory(libs)

# Add applications subdirectory
add_subdirectory(apps)
