#pragma once

namespace sc::visor::gui::iracing {

    // Start up the iRacing GUI module
    void startup();

    // Shut down the iRacing GUI module
    void shutdown();

    // Render or update the iRacing GUI content
    void emit_content();

}  // namespace sc::visor::gui::iracing
