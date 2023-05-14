// Include the header file for the animation_instance class
#include "animation_instance.h"

// This function updates the state of the animation instance
bool sc::animation_instance::update() {
    // Initialize a flag to indicate whether the animation frame has changed
    bool changed = false;

    // Get the current time
    const auto now = std::chrono::high_resolution_clock::now();

    // Calculate the elapsed time since the last update
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);

    // Update the last update time
    last_update = now;

    // If the animation is set to play and is not currently playing, initialize the time and set the playing flag
    if (play && !playing) {
        time = 0;
        playing = true;
        play = false;
    }

    // If the animation is playing, update the time and frame index
    if (playing) {
        // Calculate the time delta
        const double delta = static_cast<double>(elapsed.count()) / 1000.0;

        // Update the time
        time += delta;

        // Calculate the new frame index
        if (const auto res = texture::frame_sequence::plot_frame_index(frame_rate, frames.size(), time, loop); res && frame_i != *res) {
            frame_i = *res;
            changed = true;
        }

        // If the animation is not looping and has reached the last frame, stop playing
        if (!loop && frame_i == frames.size() - 1) playing = false;
    }

    // Return whether the animation frame has changed
    return changed;
}
