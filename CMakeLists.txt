cmake_minimum_required(VERSION 3.10)

project(simcoaches)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
include(${PROJECT_SOURCE_DIR}/scripts/cmake/utility.cmake)
include(${PROJECT_SOURCE_DIR}/scripts/cmake/cxx-standards.cmake)
include(${PROJECT_SOURCE_DIR}/precompiled/precompiled.cmake)

win32_disable_default_manifest()
cxx_enforce_17()
conan_basic_setup(TARGETS)

add_subdirectory(libs)
add_subdirectory(apps)
