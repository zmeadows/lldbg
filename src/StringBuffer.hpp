#pragma once

#include "fmt/format.h"

// Just a convenience class to ensure all fmt buffers are nul-terminated
class StringBuffer {
    fmt::memory_buffer m_buffer;

public:
    template <typename... Args>
    inline void format(const char* fmt_str, Args&&... args)
    {
        fmt::format_to(m_buffer, fmt_str, args...);
        m_buffer.push_back('\0');
    }

    inline const char* data(void) { return m_buffer.data(); }
    // std::string to_string(void) { return fmt::to_string(m_buffer); }
};
