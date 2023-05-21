add_library(api STATIC  #Add a static library named "api"
    "api.cxx"  #Compile the source file "api.cxx" to create the library
)

target_link_libraries(api  #Specify the libraries that "api" depends on
    CONAN_PKG::botan  #Link with the "botan" library managed by Conan package manager
    rest  #Link with the user-defined "rest" library
)
