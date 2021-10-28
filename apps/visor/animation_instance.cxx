#include "animation_instance.h"

bool sc::animation_instance::update() {
    bool changed = false;
    const auto now = std::chrono::high_resolution_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);
    last_update = now;
    if (play && !playing) {
        time = 0;
        playing = true;
        play = false;
    }
    if (playing) {
        const double delta = static_cast<double>(elapsed.count()) / 1000.0;
        time += delta;
        if (const auto res = texture::frame_sequence::plot_frame_index(frame_rate, frames.size(), time, loop); res && frame_i != *res) {
            frame_i = *res;
            changed = true;
        }
        if (!loop && frame_i == frames.size() - 1) playing = false;
    }
    return changed;
}