#include "application.h"

std::atomic_bool sc::visor::keep_running = true;
bool sc::visor::legacy_support_error = false;
std::optional<std::string> sc::visor::legacy_support_error_description;