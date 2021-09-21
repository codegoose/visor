#include "application.h"

std::atomic_bool sc::visor::keep_running = true;
std::vector<std::shared_ptr<sc::visor::joystick>> sc::visor::joysticks;