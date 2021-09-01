#include "sentry.h"

#include <iostream>

#include <sentry.h>

namespace sc::sentry {

    const auto options = sentry_options_new();

    bool initialize();
    void shutdown();
}

bool sc::sentry::initialize(const std::string_view &dsn) {
    sentry_options_set_dsn(options, dsn.data());
    const bool success = (sentry_init(options) == 0);
    std::clog << "Sentry: " << (success ? "Initialized" : "Failed to initialize.") << std::endl;
    return success;
}

void sc::sentry::shutdown() {
    sentry_close();
    std::clog << "Sentry: Shutdown" << std::endl;
}