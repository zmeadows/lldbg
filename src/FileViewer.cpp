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

    LexState state = LexState::Normal;
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

        std::string id = "##line_" + std::to_string(line_number);
        ImGui::Selectable(id.c_str(), selected, ImGuiSelectableFlags_AllowOverlap);

        if (ImGui::IsItemClicked())
        {
            clicked_line = line_number;
        }

        const ImVec2 row_min = ImGui::GetItemRectMin();
        const ImVec2 row_max = ImGui::GetItemRectMax();

        // Draw line number in gutter
        const std::string line_number_text = std::to_string(line_number);
        ImVec2 line_number_pos = row_min;
        line_number_pos.x += m_breakpoint_gutter_width;
        line_number_pos.y += style.FramePadding.y;

        window->DrawList->AddText(
            line_number_pos, IM_COL32(120, 120, 120, 255), line_number_text.c_str()
        );

        // Draw seperator between line numbers and code
        const float separator_x =
            row_min.x + m_breakpoint_gutter_width + m_line_number_gutter_width + 2.0f;
        window->DrawList->AddLine(
            ImVec2(separator_x, row_min.y), ImVec2(separator_x, row_max.y),
            IM_COL32(60, 60, 60, 255)
        );

        // Draw code
        ImVec2 text_pos = row_min;
        text_pos.x += m_breakpoint_gutter_width + m_line_number_gutter_width + m_gutter_padding;
        text_pos.y += style.FramePadding.y;

        const auto tokens = Lex::lex_cpp_line(std::string_view(m_lines[i]), state);

        for (const auto& token : tokens)
        {
            std::string_view text(m_lines[i].c_str() + token.start, token.length);

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

            const ImVec2 text_size = ImGui::CalcTextSize(text.data(), text.data() + text.size());
            text_pos.x += text_size.x;
        }

        // Draw breakpoint marker in gutter
        if (bps != nullptr && bps->find(int(line_number)) != bps->end())
        {
            ImVec2 bp_pos = row_min;
            bp_pos.x += m_breakpoint_gutter_width * 0.5f;
            bp_pos.y += (row_max.y - row_min.y) * 0.5f;

            window->DrawList->AddCircleFilled(bp_pos, 4.0f, IM_COL32(255, 0, 0, 255));
        }
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
