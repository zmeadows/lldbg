#pragma once

#include "imgui.h"
#include <imgui_internal.h>

bool MyTreeNode(const char* label)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    ImGuiID id = window->GetID(label);
    ImVec2 pos = window->DC.CursorPos;
    ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + g.FontSize + g.Style.FramePadding.y*2));
    bool opened = ImGui::TreeNodeBehaviorIsOpen(id);
    bool hovered, held;
    if (ImGui::ButtonBehavior(bb, id, &hovered, &held, true))
        window->DC.StateStorage->SetInt(id, opened ? 0 : 1);
    if (hovered || held)
        window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(held ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered));

    // Icon, text
    float button_sz = g.FontSize + g.Style.FramePadding.y*2;
    window->DrawList->AddRectFilled(pos, ImVec2(pos.x+button_sz, pos.y+button_sz), opened ? ImColor(51,105,173) : ImColor(42,79,130));
    ImGui::RenderText(ImVec2(pos.x + button_sz + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), label);

    ImGui::ItemSize(bb, g.Style.FramePadding.y);
    ImGui::ItemAdd(bb, id);

    if (opened)
        ImGui::TreePush(label);
    return opened;
}
