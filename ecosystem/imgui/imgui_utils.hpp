#pragma once

#include <imgui.h>

#include "imgui_freetype.h"

struct ImFreetypeEnablement {

    bool needs_rebuild = true;
    float alpha = 1;
    unsigned int build_flags = 0;

    bool PreNewFrame() {
        if (!needs_rebuild) return false;
        auto atlas = ImGui::GetIO().Fonts;
        for (int n = 0; n < atlas->ConfigData.Size; n++) ((ImFontConfig*)&atlas->ConfigData[n])->RasterizerMultiply = alpha;
        atlas->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
        atlas->FontBuilderFlags = build_flags;
        atlas->Build();
        needs_rebuild = false;
        return true;
    }
};

struct ImDrawCompare {

    std::vector<std::vector<ImDrawVert>> prev_vertices;
    std::vector<std::vector<ImDrawIdx>> prev_indices;

    bool Check(ImDrawData *const data) {
        const auto draw_list_num = data->CmdListsCount;
        const bool prev_vertices_match = [&]() {
            if (prev_vertices.size() < draw_list_num) return false;
            for (int i = 0; i < prev_vertices.size() && i < data->CmdListsCount; i++) {
                if (prev_vertices[i].size() < data->CmdLists[i]->VtxBuffer.size()) return false;
                if (memcmp(prev_vertices[i].data(), data->CmdLists[i]->VtxBuffer.Data, sizeof(ImDrawVert) * data->CmdLists[i]->VtxBuffer.size()) != 0) return false;
            }
            return true;
        }();
        const bool prev_indices_match = [&]() {
            if (prev_indices.size() < draw_list_num) return false;
            for (int i = 0; i < prev_indices.size() && i < data->CmdListsCount; i++) {
                if (prev_indices[i].size() < data->CmdLists[i]->IdxBuffer.size()) return false;
                if (memcmp(prev_indices[i].data(), data->CmdLists[i]->IdxBuffer.Data, sizeof(ImDrawIdx) * data->CmdLists[i]->IdxBuffer.size()) != 0)  return false;
            }
            return true;
        }();
        if (prev_vertices_match && prev_indices_match) return true;
        if (prev_vertices.size() < draw_list_num) prev_vertices.resize(draw_list_num);
        if (prev_indices.size() < draw_list_num) prev_indices.resize(draw_list_num);
        for (int i = 0; i < draw_list_num; i++) {
            const auto this_draw_list = data->CmdLists[i];
            const auto this_draw_list_vertex_num = this_draw_list->VtxBuffer.size();
            const auto this_draw_list_index_num = this_draw_list->IdxBuffer.size();
            if (prev_vertices[i].size() < this_draw_list_vertex_num) prev_vertices[i].resize(this_draw_list_vertex_num);
            memcpy(prev_vertices[i].data(), this_draw_list->VtxBuffer.Data, sizeof(ImDrawVert) * this_draw_list->VtxBuffer.size());
            if (prev_indices[i].size() < this_draw_list_index_num) prev_indices[i].resize(this_draw_list_index_num);
            memcpy(prev_indices[i].data(), this_draw_list->IdxBuffer.Data, sizeof(ImDrawIdx) * this_draw_list->IdxBuffer.size());
        }
        return false;
    }
};

struct ImPenUtility {

    ImVec2 content_region_size, content_region_start;
    ImVec2 window_position, window_padding;

    void CalculateWindowBounds() {
        window_position = ImGui::GetWindowPos();
        window_padding = ImGui::GetStyle().WindowPadding;
        content_region_size = ImGui::GetWindowContentRegionMax();
        content_region_start = ImVec2(window_position.x + (window_padding.x / 2), window_position.y + (window_padding.y / 2));
    }

    ImVec2 GetCenteredPosition(const ImVec2 &size) {
        return {
            content_region_start.x + (content_region_size.x / 2) - (size.x / 2),
            content_region_start.y + (content_region_size.y / 2) - (size.y / 2)
        };
    }
};