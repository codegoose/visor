add_library(hidapi STATIC
    "hid.c"
)

target_link_libraries(hidapi
    setupapi
)