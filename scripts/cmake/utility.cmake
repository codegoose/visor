# Function to disable the default manifest on Win32 platform
function(win32_disable_default_manifest)
    if(WIN32)
        set(CMAKE_SHARED_LINKER_FLAGS /MANIFEST:NO)
    endif()
endfunction()

# Function to configure Win32 release mode without console
function(win32_release_mode_no_console target)
    if(WIN32)
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_link_options(${target} PRIVATE "/SUBSYSTEM:WINDOWS")
            target_link_options(${target} PRIVATE "/ENTRY:mainCRTStartup")
        endif()
    endif()
endfunction()
