add_library(subprocess STATIC
    "CowData.cpp"
    "environ.cpp"
    "pipe.cpp"
    "PipeVar.cpp"
    "ProcessBuilder.cpp"
    "ProcessBuilder_windows.cpp"
    "shell_utils.cpp"
    "utf8_to_utf16.cpp"
)
