#pragma once

#include "FileSystem.hpp"

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

class FileViewer
{
    std::vector<std::string> m_lines;
    std::map<FileHandle, std::unordered_set<int>> m_breakpoint_cache;
    std::optional<decltype(m_breakpoint_cache)::iterator> m_breakpoints;

    std::optional<int> m_highlighted_line = {};
    bool m_highlight_line_needs_focus = false;

  public:
    void show(FileHandle handle);
    std::optional<int> render(void);
    void synchronize_breakpoint_cache(lldb::SBTarget target);

    inline void set_highlight_line(int line)
    {
        m_highlighted_line = line;
        m_highlight_line_needs_focus = true;
    }

    inline void set_highlight_line_without_focus(int line)
    {
        m_highlighted_line = line;
        m_highlight_line_needs_focus = false;
    }

    inline void unset_highlight_line()
    {
        m_highlighted_line = {};
        m_highlight_line_needs_focus = false;
    }

    inline std::optional<int> get_highlight_line()
    {
        return m_highlighted_line;
    }
};
