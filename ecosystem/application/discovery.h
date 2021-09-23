#pragma once

#include "../firmware/mk4.h"

namespace sc::visor::discovery {

    extern std::shared_ptr<firmware::mk4::device_handle> handles_mk3;

    void startup();
    void shutdown();
}