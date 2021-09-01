#include "sentry.h"

#include <iostream>

#include <sentry.h>

namespace sc::sentry {

    const auto options = sentry_options_new();
    const auto user = sentry_value_new_object();

    bool initialize();
    void shutdown();
}

bool sc::sentry::initialize(const std::string_view &dsn, const std::string_view &release) {
    sentry_options_set_dsn(options, dsn.data());
    sentry_options_set_release(options, release.data());
    const bool success = (sentry_init(options) == 0);
    std::clog << "Sentry: " << (success ? "Initialized" : "Failed to initialize.") << std::endl;
    if (success) {
        sentry_value_set_by_key(user, "ip_address", sentry_value_new_string("{{auto}}"));
        sentry_set_user(user);
        return true;
    }
    return false;
}

void sc::sentry::shutdown() {
    sentry_remove_user();
    sentry_close();
    std::clog << "Sentry: Shutdown" << std::endl;
}