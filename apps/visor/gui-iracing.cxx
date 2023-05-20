#include "gui-iracing.h"
// This includes the gui-iracing.h header file, which likely contains the declarations of the methods being defined here.

#include <imgui.h>
// This includes the imgui.h header file, which is the main header file for the Dear ImGui library, a popular library for creating graphical user interfaces in C++.

#include <spdlog/spdlog.h>
// This includes the spdlog.h header file, which is the main header file for the spdlog library, a fast C++ logging library.

#include <magic_enum.hpp>
// This includes the magic_enum.hpp header file, which is the main header file for the magic_enum library, a static reflection library in C++ for enums.

#include "../../libs/font/imgui.h"
// This includes a custom imgui.h header file, which likely contains some customizations or additions for using fonts with ImGui.

#include "../../libs/iracing/iracing.h"
// This includes the iracing.h header file, which likely contains declarations for interacting with the iRacing API.

void sc::visor::gui::iracing::startup() {
    // This is an empty method named 'startup'. It might be a placeholder for initialization tasks to be added later.
}

void sc::visor::gui::iracing::shutdown() {
    // This is an empty method named 'shutdown'. It might be a placeholder for cleanup tasks to be added later.
}

void sc::visor::gui::iracing::emit_content() {
    // This method named 'emit_content' is likely used to draw or update the ImGui interface.

    if (ImGui::BeginTabBar("iRacingTabBar")) {
        // This starts a new tab bar with the identifier "iRacingTabBar".

        if (ImGui::BeginTabItem(fmt::format("{} Telemetry", ICON_FA_SIGNAL_STREAM).data())) {
            // This starts a new tab item with the title "Telemetry". The icon is set using a font awesome icon.

            switch (sc::iracing::get_status()) {
                // This gets the current status of the iRacing connection and does different things based on the status.

                case sc::iracing::status::connected:
                    // If the status is 'connected', then it shows a green "Connected" text.
                    ImGui::TextColored({ 0.f, 1.f, 0.f, 1.f }, fmt::format("{} Connected", ICON_FA_CHECK).data());
                    break;

                case sc::iracing::status::live:
                    // If the status is 'live', then it shows a green "Live" text.
                    ImGui::TextColored({ 0.f, 1.f, 0.f, 1.f }, fmt::format("{} Live", ICON_FA_CHECK_DOUBLE).data());
                    break;

                case sc::iracing::status::searching:
                    // If the status is 'searching', then it shows a gray "Searching" text.
                    ImGui::TextDisabled(fmt::format("{} Searching", ICON_FA_SEARCH).data());
                    break;

                case sc::iracing::status::stopped:
                    // If the status is 'stopped', then it shows a gray "Paused" text.
                    ImGui::TextDisabled(fmt::format("{} Paused", ICON_FA_PAUSE).data());
                    break;
            }

            ImGui::ProgressBar(sc::iracing::lap_percent());
            // This displays a progress bar with the current lap percentage.

            ImGui::Text(fmt::format("Lap: {}%", sc::iracing::lap_percent()).data());
            // This displays the current lap percentage as text.

            ImGui::Text(fmt::format("RPM: {}", sc::iracing::rpm()).data());
           ```c++
            // This displays the current RPM (Revolutions per Minute) as text.

            ImGui::Text(fmt::format("Speed: {}", sc::iracing::speed()).data());
            // This displays the current speed as text.

            ImGui::Text(fmt::format("Gear: {}", sc::iracing::gear()).data());
            // This displays the current gear as text.

            if (ImGui::BeginChild("TelemetryVariables", { 0, 0 }, true)) {
                // This starts a new child window with the identifier "TelemetryVariables".
                // The window's size is {0, 0} which means it uses the available space.

                for (auto &var : sc::iracing::variables()) {
                    // This loops through all the variables provided by the iRacing API.

                    ImGui::Text(var.first.data());
                    // This displays the name of the variable as text.

                    ImGui::SameLine();
                    // This makes the next ImGui command appear on the same line as the previous one.

                    ImGui::TextDisabled(fmt::format("Type: {}", var.second).data());
                    // This displays the type of the variable as disabled (gray) text.
                }

                ImGui::EndChild();
                // This ends the child window.
            }

            ImGui::EndTabItem();
            // This ends the tab item.
        }

        ImGui::EndTabBar();
        // This ends the tab bar.
    }
}
