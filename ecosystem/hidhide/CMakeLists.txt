add_library(hidhide STATIC
    "hidhide.cxx"
)

target_link_libraries(hidhide
    CONAN_PKG::spdlog
    CONAN_PKG::fmt

    dinput8
    dxguid
    hid
    advapi32
    ole32
    cfgmgr32
    setupapi
)