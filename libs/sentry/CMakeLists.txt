add_library(sentry STATIC
    "sentry.cxx"
)

target_link_libraries(sentry
    CONAN_PKG::sentry-native
)