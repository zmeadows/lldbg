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

    void render(void);

    std::optional<int> highlighted_line = {};

private:
    std::unordered_set<int> m_breakpoints = {};
    std::vector<std::string> m_lines = {};
};

