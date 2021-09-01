#include "imgui.h"

#include <spdlog/spdlog.h>

bool sc::font::imgui::load(const int &size) {
    if (!ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisb.ttf", size)) {
        spdlog::error("Unable to load primary font.");
        return false;
    }
    {
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        if (!ImGui::GetIO().Fonts->AddFontFromFileTTF( FONT_ICON_FILE_NAME_FAS, size - 4, &icons_config, icons_ranges)) {
            spdlog::error("Unable to load secondary font.");
            return false;
        }
    }
    {
        static const ImWchar icons_ranges[] = { ICON_MIN_FAB, ICON_MAX_FAB, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        if (!ImGui::GetIO().Fonts->AddFontFromFileTTF( FONT_ICON_FILE_NAME_FAB, size - 4, &icons_config, icons_ranges)) {
            spdlog::error("Unable to load tertiary font.");
            return false;
        }
    }
    spdlog::debug("All fonts have been loaded.");
    return true;
}