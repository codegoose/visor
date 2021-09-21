#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#include "../hidapi/hidapi.h"

#include "mk3.h"

int main() {
    spdlog::default_logger()->set_level(spdlog::level::debug);
    const auto res = sc::firmware::mk3::discover();
    if (!res.has_value()) {
        spdlog::error(res.error());
        return 1;
    }
    for (auto device : *res) {
        const auto res = device->version();
        if (!res.has_value()) {
            spdlog::error(res.error());
            return 2;
        }
        spdlog::info("Okay: Firmware MK3 Revision #{}", *res);
    }
}