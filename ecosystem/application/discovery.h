#pragma once

#include "../firmware/mk3.h"

namespace sc::visor::discovery {

    extern std::shared_ptr<firmware::mk3::device_handle> handles_mk3;

    void startup();
    void shutdown();
}