#include "rest.h"

#include <spdlog/spdlog.h>

#include "../api/api.h"

int main() {
    for (int i = 0; i < 4; i++) {
        const auto res = eon::rest::post("http://dummy.restapiexample.com/api/v1/create", {
            { "name", "Brandon" },
            { "age", 34 }
        });
        if (res.has_value()) spdlog::info(res->dump());
        else spdlog::error(res.error());
    }
    if (const auto res = sc::api::customer::get_session_token("miranda@google.com", "abc123").get(); res) {
        spdlog::critical("Session key: {}", res->dump());
    } else spdlog::error(res.error());
    return 0;
}