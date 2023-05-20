#include "application.h" // Include the "application.h" header file
#include "gui.h" // Include the "gui.h" header file

// #define SC_FEATURE_SENTRY
#define SC_FEATURE_MINIMAL_REDRAW // Enable the minimal redraw feature
#define SC_FEATURE_ENHANCED_FONTS // Enable the enhanced fonts feature
#define SC_FEATURE_RENDER_ON_RESIZE // Enable the render on resize feature
#define SC_FEATURE_SYSTEM_TRAY // Enable the system tray feature
#define SC_FEATURE_CENTER_WINDOW // Enable the center window feature

#define SC_VIEW_INIT_W 728 // Define the initial width of the view
#define SC_VIEW_INIT_H 636 // Define the initial height of the view
#define SC_VIEW_MIN_W SC_VIEW_INIT_W // Define the minimum width of the view
#define SC_VIEW_MIN_H SC_VIEW_INIT_H // Define the minimum height of the view

#include "../../libs/boot/imgui_gl3_glfw3.hpp" // Include the imgui_gl3_glfw3 library
#include "../../libs/iracing/iracing.h" // Include the iracing library

#include "legacy.h" // Include the "legacy.h" header file

// Function to enforce only one instance of the application
static bool enforce_one_instance() {
    const auto mutex = CreateMutex(NULL, TRUE, "SimCoachesVisorEcosystemApplication"); // Create a mutex with a specific name
    if (auto mutex_wait_res = WaitForSingleObject(mutex, 0); mutex_wait_res != WAIT_OBJECT_0) { // Wait for the mutex to be signaled
        MessageBox(NULL, "It seems like Visor is already running. Look for the icon on your taskbar.", "Visor", MB_OK | MB_ICONINFORMATION); // Show a message box indicating duplicate instance
        return false; // Return false to indicate duplicate instance
    } else return true; // Return true if the mutex was successfully obtained
}

// Function called during application startup
static std::optional<std::string> sc::boot::on_startup() {
    if (!enforce_one_instance()) return "Duplicate instance."; // Check if it's a duplicate instance and return an error message if true
    // iracing::startup(); // Perform startup actions for the iracing module
    visor::gui::initialize(); // Initialize the GUI module
    return std::nullopt; // Return no error
}

// Function called during fixed update
static tl::expected<bool, std::string> sc::boot::on_fixed_update() {
    return true; // Return true to continue execution
}

// Function called during update
static tl::expected<bool, std::string> sc::boot::on_update(const glm::ivec2 &framebuffer_size, bool *const force_redraw) {
    visor::gui::emit(framebuffer_size, force_redraw); // Emit GUI events passing framebuffer size and force_redraw flag
    if (const auto err = visor::legacy::process(); err) { // Process legacy support and check for errors
        sc::visor::legacy_support_error = true; // Set the legacy support error flag
        sc::visor::legacy_support_error_description = *err; // Set the legacy support error description
    }
    return visor::keep_running; // Return the value of keep_running flag
}

// Function called during application shutdown
static void sc::boot::on_shutdown() {
    visor::gui::shutdown(); // Shutdown the GUI module
    // iracing::shutdown(); // Perform shutdown actions for the iracing module
    visor::legacy::disable(); // Disable legacy support
}
