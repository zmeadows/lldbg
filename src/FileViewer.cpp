#include "FileViewer.hpp"

#include "StringBuffer.hpp"

// clang-format off
#include "imgui.h"
#include "imgui_internal.h"
// clang-format on

void FileViewer::render(void)
{
    const std::unordered_set<int>* const bps =
        (m_breakpoints.has_value() && m_breakpoints != m_breakpoint_cache.end())
            ? &(*m_breakpoints)->second
            : nullptr;

    // style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.f, 0.00f, 0.00f, 1.00f);
    // ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(255, 0, 0, 100));

    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    StringBuffer line_buffer;
    for (size_t i = 0; i < m_lines.size(); i++) {
        const size_t line_number = i + 1;

        line_buffer.format("   {}  {}\n", line_number, m_lines[i]);

        if (bps != nullptr && bps->find(line_number) != bps->end()) {
            ImVec2 pos = window->DC.CursorPos;
            ImVec2 txt = ImGui::CalcTextSize("X");
            float radius = txt.y / 2.5f;
            pos.x += 1.5 * txt.x;
            pos.y += txt.y / 2.f;
            window->DrawList->AddCircleFilled(pos, radius, IM_COL32(255, 0, 0, 255));
        }

        if (m_highlighted_line.has_value() &&
            line_number == static_cast<size_t>(*m_highlighted_line)) {
            ImVec2 pos = window->DC.CursorPos;
            ImVec2 txt = ImGui::CalcTextSize(line_buffer.data());

            ImVec2 vMax = ImGui::GetWindowContentRegionMax();
            vMax.x += ImGui::GetWindowPos().x;

            ImRect bb(pos, ImVec2(vMax.x, pos.y + txt.y));

            window->DrawList->AddRectFilled(bb.Min, bb.Max, IM_COL32(125, 125, 125, 100));

            if (m_highlight_line_needs_focus) {
                ImGui::SetScrollHere();
                m_highlight_line_needs_focus = false;
            }
        }

        ImGui::Selectable(line_buffer.data());
        if (ImGui::IsItemClicked()) {
            LOG(Verbose) << "clicked line: " << line_number;
        }

        line_buffer.clear();
    }
}

void FileViewer::synchronize_breakpoint_cache(lldb::SBTarget target)
{
    for (auto& [_, bps] : m_breakpoint_cache) bps.clear();

    for (uint32_t i = 0; i < target.GetNumBreakpoints(); i++) {
        lldb::SBBreakpoint bp = target.GetBreakpointAtIndex(i);
        lldb::SBBreakpointLocation location = bp.GetLocationAtIndex(0);

        if (!location.IsValid()) {
            LOG(Error) << "Invalid breakpoint location encountered by LLDB.";
        }

        lldb::SBAddress address = location.GetAddress();

        if (!address.IsValid()) {
            LOG(Error) << "Invalid lldb::SBAddress for breakpoint encountered by LLDB.";
        }

        lldb::SBLineEntry line_entry = address.GetLineEntry();

        const std::string bp_filepath =
            fmt::format("{}/{}", line_entry.GetFileSpec().GetDirectory(),
                        line_entry.GetFileSpec().GetFilename());

        const auto maybe_handle = FileHandle::create(bp_filepath);
        if (!maybe_handle) {
            LOG(Error) << "Invalid filepath found for breakpoint: " << bp_filepath;
            continue;
        }
        const FileHandle handle = *maybe_handle;

        if (auto it = m_breakpoint_cache.find(handle); it == m_breakpoint_cache.end()) {
            m_breakpoint_cache.emplace(handle,
                                       std::unordered_set<int>({(int)line_entry.GetLine()}));
        }
        else {
            it->second.insert((int)line_entry.GetLine());
        }
    }
}

void FileViewer::show(FileHandle handle)
{
    m_lines = handle.contents();

    if (const auto it = m_breakpoint_cache.find(handle); it != m_breakpoint_cache.end()) {
        m_breakpoints = it;
    }
    else {
        m_breakpoints = {};
    }
}
