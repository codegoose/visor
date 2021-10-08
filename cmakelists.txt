cmake_minimum_required(VERSION 3.10)

project(simcoaches)

message("Project Source Directory: ${PROJECT_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)

conan_basic_setup(TARGETS)

include_directories("${PROJECT_SOURCE_DIR}/ecosystem/precompiled/rlottie/include")

if(CMAKE_BUILD_TYPE MATCHES Debug)
    message("Building in debug mode.")
    add_compile_definitions(EON_DEBUG)
    link_libraries("${PROJECT_SOURCE_DIR}/ecosystem/precompiled/rlottie/lib/Debug/rlottie.lib")
else()
    message("Not debug mode.")
    add_compile_definitions(EON_RELEASE)
    link_libraries("${PROJECT_SOURCE_DIR}/ecosystem/precompiled/rlottie/lib/RelWithDebInfo/rlottie.lib")
endif()

add_subdirectory(ecosystem)