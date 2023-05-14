// Include necessary headers
#include "gui-iracing.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <magic_enum.hpp>
#include "../../libs/font/imgui.h"
#include "../../libs/iracing/iracing.h"

// This function is called when the iRacing GUI component is initialized
void sc::visor::gui::iracing::startup() {

}

// This function is called when the iRacing GUI component is shut down
void sc::visor::gui::iracing::shutdown() {

}

// This function is responsible for rendering the iRacing GUI content
void sc::visor::gui::iracing::emit_content() {
    // Begin a new tab bar
    if (ImGui::BeginTabBar("iRacingTabBar")) {
        // Begin a new tab item for telemetry
        if (ImGui::BeginTabItem(fmt::format("{} Telemetry", ICON_FA_SIGNAL_STREAM).c_str())) {
            // Check the status of the iRacing component and display different text based on the status
            switch (sc::iracing::get_status()) {
                case sc::iracing::status::connected:
                    ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), fmt::format("{} Connected", ICON_FA_CHECK).c_str());
                    break;
                case sc::iracing::status::live:
                    ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), fmt::format("{} Live", ICON_FA_CHECK_DOUBLE).c_str());
                    break;
                case sc::iracing::status::searching:
                    ImGui::TextDisabled(fmt::format("{} Searching", ICON_FA_SEARCH).c_str());
                    break;
                case sc::iracing::status::stopped:
                    ImGui::TextDisabled(fmt::format("{} Paused", ICON_FA_PAUSE).c_str());
                    break;
            }
            // Display a progress bar for the lap percent
            ImGui::ProgressBar(sc::iracing::lap_percent());
            // Display text for various telemetry data
            ImGui::Text(fmt::format("Lap: {:.1f}%", sc::iracing::lap_percent()).c_str());
            ImGui::Text(fmt::format("RPM: {:.0f}", sc::iracing::rpm()).c_str());
            ImGui::Text(fmt::format("Speed: {:.0f} mph", sc::iracing::speed()).c_str());
            ImGui::Text(fmt::format("Gear: {}", sc::iracing::gear()).c_str());
            // Begin a new child window with a list of telemetry variables
            if (ImGui::BeginChild("TelemetryVariables", ImVec2(0, 0), true)) {
                for (const auto& [name, type] : sc::iracing::variables()) {
                    ImGui::Text(name.c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled(fmt::format("Type: {}", type).c_str());
                }
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
