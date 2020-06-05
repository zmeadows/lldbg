#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

class FileViewer {
public:
    inline void set_lines(const std::vector<std::string>& new_lines) { m_lines = new_lines; }

    inline void set_breakpoints(const std::unordered_set<int>* new_breakpoints)
    {
        if (new_breakpoints != nullptr) {
            m_breakpoints = *new_breakpoints;
        }
        else {
            m_breakpoints = {};
        }
    }

    inline void set_highlighted_line(int line_number) { m_highlighted_line = line_number; }
    inline void unset_highlighted_line(void) { m_highlighted_line = {}; }

    void render(void);

private:
    std::optional<int> m_highlighted_line = {};
    std::unordered_set<int> m_breakpoints = {};
    std::vector<std::string> m_lines = {};
};

