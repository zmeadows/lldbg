#include "FileViewer.hpp"

#include "StringBuffer.hpp"

// clang-format off
#include "imgui.h"
#include "imgui_internal.h"
// clang-format on

void FileViewer::render(void)
{
    StringBuffer line_buffer;
    for (size_t i = 0; i < m_lines.size(); i++) {
        const size_t line_number = i + 1;
        if (m_breakpoints.find(line_number) != m_breakpoints.end()) {
            line_buffer.format(" X {}  {}\n", line_number, m_lines[i]);
        }
        else {
            line_buffer.format("   {}  {}\n", line_number, m_lines[i]);
        }

        if (highlighted_line.has_value() && i == static_cast<size_t>(*highlighted_line)) {
            ImGuiContext& g = *GImGui;
            ImGuiWindow* window = g.CurrentWindow;

            ImVec2 pos = window->DC.CursorPos;
            ImVec2 txt = ImGui::CalcTextSize(line_buffer.data());

            ImVec2 vMax = ImGui::GetWindowContentRegionMax();
            vMax.x += ImGui::GetWindowPos().x;

            ImRect bb(pos, ImVec2(vMax.x, pos.y + txt.y));

            window->DrawList->AddRectFilled(bb.Min, bb.Max,
                                            ImGui::GetColorU32(ImGuiCol_HeaderActive));
            ImGui::TextUnformatted(line_buffer.data());

            ImGui::SetScrollHere();
        }
        else {
            ImGui::TextUnformatted(line_buffer.data());
        }

        line_buffer.clear();
    }
}
