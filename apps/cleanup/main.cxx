#include "main.rc"

#include <iostream>
#include <sstream>

#include <windows.h>

int main(int arg_c, char **arg_v) {
    std::cout << "Copyright " << VER_LEGAL_COPYRIGHT << std::endl;
    std::cout << VER_APP_NAME << " (" << VER_APP_VER << ")" << std::endl;
    std::cout << VER_APP_DESCRIPTION << std::endl;
    #ifdef NDEBUG
    std::cout << "This application was compiled in release mode." << std::endl;
    #else
    std::cout << "This application was compiled in debug mode." << std::endl;
    #endif
    std::cout << "There " << (arg_c == 1 ? "was" : "were") << " " << arg_c << " argument" << (arg_c == 1 ? "" : "s") << " passed." << std::endl;
    for (int i = 0; i < arg_c; i++) std::cout << "  [" << i << "] \"" << arg_v[i] << "\"" << std::endl;
    MessageBoxA(nullptr, "pause", "Info", MB_OK);
    return 0;
}
