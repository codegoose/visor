#include "animation_instance.h"
// This includes the animation_instance.h header file, which contains the definition of the animation_instance structure.

bool sc::animation_instance::update() {
    // This function updates the state of an animation_instance. It returns a boolean indicating whether the animation frame has changed.

    bool changed = false;
    // This flag is set to true if the frame of the animation changes during this update.

    const auto now = std::chrono::high_resolution_clock::now();
    // This gets the current time.

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);
    // This calculates the time elapsed since the last update, in milliseconds.

    last_update = now;
    // This updates the last_update time point to the current time.

    if (play && !playing) {
        // If the animation is set to play and is not currently playing...

        time = 0;
        // ...reset the animation time to 0...

        playing = true;
        // ...start the animation playing...

        play = false;
        // ...and reset the play flag.
    }
    
    if (playing) {
        // If the animation is currently playing...

        const double delta = static_cast<double>(elapsed.count()) / 1000.0;
        // ...calculate the time delta since the last update, in seconds...

        time += delta;
        // ...and add the delta to the animation time.

        if (const auto res = texture::frame_sequence::plot_frame_index(frame_rate, frames.size(), time, loop); res && frame_i != *res) {
            // If the result of the frame_sequence's plot_frame_index function is a valid frame index that is different from the current frame index...

            frame_i = *res;
            // ...update the frame index...

            changed = true;
            // ...and set the changed flag to true.
        }
        if (!loop && frame_i == frames.size() - 1) playing = false;
        // If the animation is not set to loop and the current frame is the last frame, stop the animation.
    }
    
    return changed;
    // Return the changed flag, indicating whether the animation frame has changed during this update.
}
