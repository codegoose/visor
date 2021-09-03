#include "imgui.h"

#include "../resource/resource.h"

#include <spdlog/spdlog.h>

bool sc::font::imgui::load(const int &size) {
    {
        ImFontConfig font_config;
        font_config.FontDataOwnedByAtlas = false;
        if (static const auto rsc_font_1 = resource::get_resource("DATA", "FONT_1"); !rsc_font_1 || !ImGui::GetIO().Fonts->AddFontFromMemoryTTF(rsc_font_1->first, rsc_font_1->second, size, &font_config)) {
            spdlog::error("Unable to load primary font.");
            return false;
        }
    }
    {
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.FontDataOwnedByAtlas = false;
        if (static const auto rsc_font_2 = resource::get_resource("DATA", "FONT_2"); !rsc_font_2 || !ImGui::GetIO().Fonts->AddFontFromMemoryTTF(rsc_font_2->first, rsc_font_2->second, size - 4, &icons_config, icons_ranges)) {
            spdlog::error("Unable to load secondary font.");
            return false;
        }
    }
    {
        static const ImWchar icons_ranges[] = { ICON_MIN_FAB, ICON_MAX_FAB, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.FontDataOwnedByAtlas = false;
        if (static const auto rsc_font_3 = resource::get_resource("DATA", "FONT_3"); !rsc_font_3 || !ImGui::GetIO().Fonts->AddFontFromMemoryTTF(rsc_font_3->first, rsc_font_3->second, size - 4, &icons_config, icons_ranges)) {
            spdlog::error("Unable to load tertiary font.");
            return false;
        }
    }
    spdlog::debug("All fonts have been loaded.");
    return true;
}