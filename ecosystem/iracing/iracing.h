#pragma once

namespace sc::iracing {

    enum class status {

        stopped,
        searching,
        connected,
        live
    };

    void startup();
    void shutdown();

    status get_status();
}