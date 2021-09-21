#include "firmware.h"

#include "../hidapi/hidapi.h"

#include <mutex>

std::optional<std::string> sc::firmware::prepare_subsystem() {
    static std::mutex mutex;
    std::lock_guard guard(mutex);
    if (hid_init() == 0) return std::nullopt;
    return "Unable to initialize HID API.";
}