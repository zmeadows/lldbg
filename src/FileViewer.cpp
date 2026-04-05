#include "FileViewer.hpp"

#include "Defer.hpp"
#include "Lex.hpp"
#include "StringBuffer.hpp"

// clang-format off
#include "imgui.h"
#include "imgui_internal.h"
// clang-format on

std::optional<int> FileViewer::render()
{
    const std::unordered_set<int>* const bps =
        (m_breakpoints.has_value() && m_breakpoints != m_breakpoint_cache.end())
            ? &(*m_breakpoints)->second
            : nullptr;

    ImGuiContext& g = *GImGui;
    auto& style = g.Style;
    ImGuiWindow* window = g.CurrentWindow;

    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style.Colors[ImGuiCol_TitleBg]);
    Defer(ImGui::PopStyleColor());
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.Colors[ImGuiCol_TitleBg]);
    Defer(ImGui::PopStyleColor());

    std::optional<int> clicked_line = {};

    for (size_t i = 0; i < m_lines.size(); i++)
    {
        const size_t line_number = i + 1;

        bool selected = false;
        if (m_highlighted_line.has_value() &&
            line_number == static_cast<size_t>(*m_highlighted_line))
        {
            selected = true;
            if (m_highlight_line_needs_focus)
            {
                ImGui::SetScrollHereY();
                m_highlight_line_needs_focus = false;
            }
        }

        StringBuffer line_buffer;
        line_buffer.format("   {}  {}", line_number, m_lines[i]);

        // 1. Invisible selectable row
        std::string id = "##line_" + std::to_string(line_number);
        ImGui::Selectable(id.c_str(), selected, ImGuiSelectableFlags_AllowOverlap);

        if (ImGui::IsItemClicked())
        {
            clicked_line = line_number;
        }

        const ImVec2 row_min = ImGui::GetItemRectMin();
        const ImVec2 row_max = ImGui::GetItemRectMax();

        // 2. Draw syntax-highlighted text manually on top of the row
        ImVec2 text_pos = row_min;
        text_pos.x += style.FramePadding.x;
        text_pos.y += style.FramePadding.y;

        const auto tokens = Lex::lex_cpp_line(line_buffer.view());

        for (const auto& token : tokens)
        {
            std::string_view text(line_buffer.data() + token.start, token.length);

            ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
            switch (token.kind)
            {
            case TokenKind::Keyword:
                color = IM_COL32(86, 156, 214, 255);
                break;
            case TokenKind::String:
            case TokenKind::Character:
                color = IM_COL32(206, 145, 120, 255);
                break;
            case TokenKind::Comment:
                color = IM_COL32(106, 153, 85, 255);
                break;
            case TokenKind::Number:
                color = IM_COL32(181, 206, 168, 255);
                break;
            case TokenKind::Preprocessor:
                color = IM_COL32(155, 155, 0, 255);
                break;
            default:
                break;
            }

            window->DrawList->AddText(text_pos, color, text.data(), text.data() + text.size());

            ImVec2 text_size = ImGui::CalcTextSize(text.data(), text.data() + text.size());
            text_pos.x += text_size.x;
        }

        // 3. Breakpoint marker
        if (bps != nullptr && bps->find(int(line_number)) != bps->end())
        {
            ImVec2 txt = ImGui::CalcTextSize("X");
            ImVec2 bp_pos = row_min;
            bp_pos.x += 1.3f * txt.x;
            bp_pos.y += (row_max.y - row_min.y) * 0.5f;

            window->DrawList->AddCircleFilled(bp_pos, txt.y / 3.f, IM_COL32(255, 0, 0, 255));
        }

        line_buffer.clear();
    }

    return clicked_line;
}

void FileViewer::synchronize_breakpoint_cache(const lldb::SBTarget& target)
{
    for (auto& [_, bps] : m_breakpoint_cache)
    {
        bps.clear();
    }

    for (uint32_t i = 0; i < target.GetNumBreakpoints(); i++)
    {
        lldb::SBBreakpoint bp = target.GetBreakpointAtIndex(i);
        lldb::SBBreakpointLocation location = bp.GetLocationAtIndex(0);

        if (!location.IsValid())
        {
            LOG(Error) << "Invalid breakpoint location encountered by LLDB.";
        }

        lldb::SBAddress address = location.GetAddress();

        if (!address.IsValid())
        {
            LOG(Error) << "Invalid lldb::SBAddress for breakpoint encountered.";
        }

        lldb::SBLineEntry line_entry = address.GetLineEntry();

        if (!line_entry.IsValid())
        {
            LOG(Error) << "Invalid lldb::SBLineEntry for breakpoint encountered.";
            continue;
        }

        // auto file_spec = line_entry.GetFileSpec();

        const std::string bp_filepath = fmt::format(
            "{}/{}", line_entry.GetFileSpec().GetDirectory(), line_entry.GetFileSpec().GetFilename()
        );

        const auto maybe_handle = FileHandle::create(bp_filepath);
        if (!maybe_handle)
        {
            LOG(Error) << "Invalid filepath found for breakpoint: " << bp_filepath;
            continue;
        }
        const FileHandle handle = *maybe_handle;

        if (auto it = m_breakpoint_cache.find(handle); it == m_breakpoint_cache.end())
        {
            m_breakpoint_cache.emplace(
                handle, std::unordered_set<int>({(int) line_entry.GetLine()})
            );
        }
        else
        {
            it->second.insert((int) line_entry.GetLine());
        }
    }
}

void FileViewer::show(FileHandle handle)
{
    m_lines = handle.contents();
    LOG(Debug) << "Showing file: " << handle.filepath();

    if (const auto it = m_breakpoint_cache.find(handle); it != m_breakpoint_cache.end())
    {
        m_breakpoints = it;
    }
    else
    {
        m_breakpoints = {};
    }
}
