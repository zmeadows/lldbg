#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "FileSystem.hpp"

class FileViewer {
    std::vector<std::string> m_lines = {};
    std::map<FileHandle, std::unordered_set<int>> m_breakpoint_cache;
    std::optional<decltype(m_breakpoint_cache)::iterator> m_breakpoints;

public:
    void show(FileHandle handle);
    void render(void);
    void synchronize_breakpoint_cache(lldb::SBTarget target);

    std::optional<int> highlighted_line = {};
};

