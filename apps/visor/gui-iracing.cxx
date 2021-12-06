#include "gui-iracing.h"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <magic_enum.hpp>

#include "../../libs/font/imgui.h"
#include "../../libs/iracing/iracing.h"

void sc::visor::gui::iracing::startup() {

}

void sc::visor::gui::iracing::shutdown() {

}

void sc::visor::gui::iracing::emit_content() {
    if (ImGui::BeginTabBar("iRacingTabBar")) {
        if (ImGui::BeginTabItem(fmt::format("{} Telemetry", ICON_FA_SIGNAL_STREAM).data())) {
            switch (sc::iracing::get_status()) {
                case sc::iracing::status::connected:
                    ImGui::TextColored({ 0.f, 1.f, 0.f, 1.f }, fmt::format("{} Connected", ICON_FA_CHECK).data());
                    break;
                case sc::iracing::status::live:
                    ImGui::TextColored({ 0.f, 1.f, 0.f, 1.f }, fmt::format("{} Live", ICON_FA_CHECK_DOUBLE).data());
                    break;
                case sc::iracing::status::searching:
                    ImGui::TextDisabled(fmt::format("{} Searching", ICON_FA_SEARCH).data());
                    break;
                case sc::iracing::status::stopped:
                    ImGui::TextDisabled(fmt::format("{} Paused", ICON_FA_PAUSE).data());
                    break;
            }
            ImGui::ProgressBar(sc::iracing::lap_percent());
            ImGui::Text(fmt::format("Lap: {}%", sc::iracing::lap_percent()).data());
            ImGui::Text(fmt::format("RPM: {}", sc::iracing::rpm()).data());
            ImGui::Text(fmt::format("Speed: {}", sc::iracing::speed()).data());
            ImGui::Text(fmt::format("Gear: {}", sc::iracing::gear()).data());
            if (ImGui::BeginChild("TelemetryVariables", { 0, 0 }, true)) {
                for (auto &var : sc::iracing::variables()) {
                    ImGui::Text(var.first.data());
                    ImGui::SameLine();
                    ImGui::TextDisabled(fmt::format("Type: {}", var.second).data());
                }
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
