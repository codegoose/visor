#include "joystick.h"

sc::visor::joystick::joystick(const SDL_JoystickID &instance_id) : instance_id(instance_id) {

}

bool sc::visor::joystick::load() {
    return false;
}

bool sc::visor::joystick::save() {
    return false;
}