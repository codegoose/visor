function(win32_disable_default_manifest)
    if(WIN32)
        set(CMAKE_SHARED_LINKER_FLAGS /MANIFEST:NO)
    endif()
endfunction()


function(win32_require_admin target)
    if(WIN32)
        add_custom_command(
            TARGET ${target}
            POST_BUILD
            COMMAND "mt.exe" -manifest \"$(SolutionDir)..\\..\\..\\apps\\need_admin.manifest\" -outputresource:"$(TargetDir)$(TargetFileName)"\;\#1
            COMMENT "Adding admin manifest to '$(TargetFileName)' ..." 
        )
    endif()
endfunction()

function(win32_release_mode_no_console target)
    if(WIN32)
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_link_options(${target} PRIVATE "/SUBSYSTEM:WINDOWS")
            target_link_options(${target} PRIVATE "/ENTRY:mainCRTStartup")
        endif()
    endif()
endfunction()